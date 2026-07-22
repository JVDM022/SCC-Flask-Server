from __future__ import annotations

from datetime import datetime, timedelta

from flask import Blueprint, current_app, jsonify, request

from ..alarms.lifecycle import sync_active_alarms
from ..alarms.rules import evaluate_alarms
from ..database.db import db
from ..database.models import Alarm, ControlCommand, Device, Event, MpcRecommendation, MqttMessageLog, PumpCycle, Telemetry
from ..ml.predictor import ThermalPredictor
from ..mpc.advisory import recommend_heater_pwm
from ..services.firmware_ota import record_event, status_summary, upsert_heartbeat
from ..services.mqtt_subscriber import mqtt_status
from ..services.notification_email import notify_critical_alarms
from ..services.pump_cycles import extract_pump_cycle_from_event
from ..services.telemetry_parser import TELEMETRY_COLUMNS, parse_csv_batch
from .auth import require_api_key

api = Blueprint("api", __name__, url_prefix="/api")

EVENT_NAMES = {
    0: "normal sample",
    1: "pump start",
    2: "pump end",
    3: "pump recovered",
    4: "hard kill",
    5: "sensor fault",
}


def _filter_telemetry_payload(payload: dict) -> dict:
    if not any(key in payload for key in TELEMETRY_COLUMNS):
        raise ValueError("Telemetry payload did not include any telemetry fields.")
    row = {key: payload.get(key) for key in TELEMETRY_COLUMNS}
    for key in ("site_id", "rig_id", "device_id", "mqtt_topic", "raw_payload"):
        if key in payload:
            row[key] = payload[key]
    return row


def _store_telemetry(row: dict, commit: bool = True) -> Telemetry:
    telemetry = Telemetry(**_filter_telemetry_payload(row))
    db.session.add(telemetry)
    db.session.flush()

    event_code = row.get("event")
    if event_code is not None and int(event_code) != 0:
        event_code = int(event_code)
        event_name = EVENT_NAMES.get(event_code, "unknown")
        db.session.add(
            Event(
                event_code=event_code,
                event_name=event_name,
                severity="critical" if event_code in {4, 5} else "info",
                message=f"ESP32 event: {event_name}",
                telemetry_id=telemetry.id,
            )
        )

    alarms = evaluate_alarms(row)
    sync_active_alarms(telemetry, alarms)

    extract_pump_cycle_from_event(telemetry)
    if commit:
        db.session.commit()
        notify_critical_alarms(current_app._get_current_object(), telemetry, alarms)
    return telemetry


@api.get("/health")
def health():
    return jsonify({"status": "ok"})


@api.get("/mqtt/status")
def api_mqtt_status():
    status = mqtt_status()
    status.update(
        {
            "host": current_app.config["MQTT_HOST"],
            "port": current_app.config["MQTT_PORT"],
            "client_id": current_app.config["MQTT_CLIENT_ID"],
            "base_topic": current_app.config["MQTT_BASE_TOPIC"],
        }
    )
    return jsonify(status)


@api.get("/mqtt/messages")
def api_mqtt_messages():
    limit = request.args.get("limit", default=100, type=int)
    rows = MqttMessageLog.query.order_by(MqttMessageLog.created_at.desc(), MqttMessageLog.id.desc()).limit(max(1, min(limit, 500))).all()
    return jsonify([row.to_dict() for row in rows])


@api.get("/devices")
def api_devices():
    rows = Device.query.order_by(Device.updated_at.desc(), Device.device_id.asc()).all()
    return jsonify([row.to_dict() for row in rows])


@api.get("/devices/<device_id>")
def api_device_detail(device_id: str):
    device = db.session.get(Device, device_id)
    if device is None:
        return jsonify({"status": "not_found", "message": f"Device {device_id} was not found."}), 404
    return jsonify(device.to_dict())


@api.get("/telemetry/latest")
def api_telemetry_latest():
    telemetry = Telemetry.query.order_by(Telemetry.created_at.desc(), Telemetry.id.desc()).first()
    return jsonify(telemetry.to_dict() if telemetry else {"status": "unavailable"})


def firmware_payload() -> tuple[dict, tuple | None]:
    payload = request.get_json(silent=True)
    if not isinstance(payload, dict):
        return {}, (jsonify({"status": "error", "message": "Expected JSON object."}), 400)
    return payload, None


def firmware_response(work):
    try:
        result = work()
        return jsonify(result)
    except ValueError as exc:
        db.session.rollback()
        return jsonify({"status": "error", "message": str(exc)}), 400


@api.post("/telemetry")
@require_api_key
def post_telemetry():
    payload = request.get_json(silent=True)
    if not isinstance(payload, dict):
        return jsonify({"status": "error", "message": "Expected JSON telemetry object."}), 400
    try:
        telemetry = _store_telemetry(payload)
    except ValueError as exc:
        db.session.rollback()
        return jsonify({"status": "error", "message": str(exc)}), 400
    return jsonify({"status": "ok", "id": telemetry.id})


@api.get("/firmware/devices")
def firmware_devices():
    return jsonify({"devices": status_summary()["devices"]})


@api.get("/firmware/status")
def firmware_status():
    return jsonify(status_summary())


@api.post("/firmware/heartbeat")
@require_api_key
def firmware_heartbeat():
    payload, error = firmware_payload()
    if error:
        return error
    return firmware_response(lambda: upsert_heartbeat(payload).to_dict())


@api.post("/firmware/ota-started")
@require_api_key
def firmware_ota_started():
    payload, error = firmware_payload()
    if error:
        return error
    return firmware_response(lambda: record_event(payload, "ota_started", "OTA started").to_dict())


@api.post("/firmware/ota-complete")
@require_api_key
def firmware_ota_complete():
    payload, error = firmware_payload()
    if error:
        return error
    return firmware_response(lambda: record_event(payload, "ota_complete", payload.get("ota_status") or "Version verified").to_dict())


@api.post("/firmware/ota-failed")
@require_api_key
def firmware_ota_failed():
    payload, error = firmware_payload()
    if error:
        return error
    return firmware_response(lambda: record_event(payload, "ota_failed", "Failed").to_dict())


@api.post("/telemetry/csv")
@require_api_key
def post_telemetry_csv():
    text = request.get_data(as_text=True)
    if not text and request.is_json:
        body = request.get_json(silent=True) or {}
        text = body.get("csv", "")
    try:
        rows = parse_csv_batch(text)
        telemetry_ids = [_store_telemetry(row, commit=False).id for row in rows]
        db.session.commit()
    except ValueError as exc:
        db.session.rollback()
        return jsonify({"status": "error", "message": str(exc)}), 400
    return jsonify({"status": "ok", "rows_stored": len(telemetry_ids), "ids": telemetry_ids})


@api.get("/latest")
def latest():
    telemetry = Telemetry.query.order_by(Telemetry.created_at.desc(), Telemetry.id.desc()).first()
    return jsonify(telemetry.to_dict() if telemetry else {"status": "unavailable"})


@api.get("/history")
def history():
    minutes = request.args.get("minutes", default=60, type=int)
    since = datetime.utcnow() - timedelta(minutes=max(1, minutes))
    rows = (
        Telemetry.query.filter(Telemetry.created_at >= since)
        .order_by(Telemetry.created_at.asc(), Telemetry.id.asc())
        .all()
    )
    return jsonify([row.to_dict() for row in rows])


@api.get("/events")
def events():
    limit = request.args.get("limit", default=100, type=int)
    rows = Event.query.order_by(Event.created_at.desc(), Event.id.desc()).limit(max(1, limit)).all()
    return jsonify([row.to_dict() for row in rows])


@api.get("/alarms/active")
def active_alarms():
    rows = Alarm.query.filter_by(active=True).order_by(Alarm.created_at.desc(), Alarm.id.desc()).all()
    return jsonify([row.to_dict() for row in rows])


@api.get("/pump-cycles")
def pump_cycles():
    rows = PumpCycle.query.order_by(PumpCycle.id.desc()).limit(200).all()
    return jsonify([row.to_dict() for row in rows])


@api.get("/ml/prediction")
def ml_prediction():
    telemetry = Telemetry.query.order_by(Telemetry.created_at.desc(), Telemetry.id.desc()).first()
    if telemetry is None:
        return jsonify({"status": "unavailable", "message": "No telemetry rows are available."})
    predictor = ThermalPredictor(current_app.config["MODEL_DIR"])
    return jsonify(predictor.predict_all(telemetry.to_dict()))


@api.get("/mpc/recommendation")
def mpc_recommendation():
    telemetry = Telemetry.query.order_by(Telemetry.created_at.desc(), Telemetry.id.desc()).first()
    if telemetry is None:
        return jsonify({"status": "unavailable", "message": "No telemetry rows are available."})
    predictor = ThermalPredictor(current_app.config["MODEL_DIR"])
    result = recommend_heater_pwm(telemetry.to_dict(), predictor=predictor)
    db.session.add(
        MpcRecommendation(
            current_temp_c=telemetry.temp_c,
            setpoint_c=telemetry.setpoint_c,
            current_pwm=telemetry.heater_pwm,
            recommended_pwm=result.recommended_pwm,
            predicted_temp_c=result.predicted_temp_c,
            cost=result.cost,
        )
    )
    db.session.commit()
    return jsonify(result.to_dict())


@api.post("/control/setpoint")
@require_api_key
def control_setpoint():
    payload = request.get_json(silent=True) or {}
    try:
        setpoint_c = float(payload["setpoint_c"])
    except (KeyError, TypeError, ValueError):
        return jsonify({"status": "error", "message": "setpoint_c must be provided as a number."}), 400

    command = ControlCommand(command_type="SETPOINT", value=0, setpoint_c=setpoint_c)
    db.session.add(command)
    db.session.commit()
    return jsonify(
        {
            "status": "ok",
            "id": command.id,
            "setpoint_c": setpoint_c,
            "warning": "ESP32 firmware safety limits, heater lockout, manual kill, and hard kill still dominate.",
        }
    )


@api.post("/control/manual-kill")
@require_api_key
def control_manual_kill():
    payload = request.get_json(silent=True) or {}
    enabled = bool(payload.get("enabled", True))
    command = ControlCommand(command_type="KILL", value=1 if enabled else 0, setpoint_c=0.0)
    db.session.add(command)
    db.session.commit()
    return jsonify(
        {
            "status": "queued",
            "id": command.id,
            "type": "KILL",
            "value": command.value,
            "manual_kill": enabled,
            "warning": "Manual kill commands are polled by the Intel NUC gateway and enforced by Arduino firmware.",
        }
    )


@api.post("/control/power")
@require_api_key
def control_power():
    payload = request.get_json(silent=True) or {}
    enabled = bool(payload.get("enabled", True))
    command = ControlCommand(command_type="SET_ON", value=1 if enabled else 0, setpoint_c=0.0)
    db.session.add(command)
    db.session.commit()
    return jsonify(
        {
            "status": "queued",
            "id": command.id,
            "type": "SET_ON",
            "value": command.value,
            "enabled": enabled,
            "warning": "Power commands are polled by the Intel NUC gateway and enforced by Arduino firmware safety limits.",
        }
    )
