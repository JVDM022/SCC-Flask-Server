from __future__ import annotations

import json
import logging
import threading
from dataclasses import dataclass
from datetime import datetime
from typing import Any

import paho.mqtt.client as mqtt
from flask import Flask

from ..alarms.rules import evaluate_alarms
from ..database.db import db
from ..database.models import (
    Alarm,
    Device,
    Event,
    FirmwareUpdateEvent,
    MlPredictionRecord,
    MpcRecommendation,
    MqttMessageLog,
    Telemetry,
)
from ..ml.predictor import ThermalPredictor
from ..mpc.advisory import recommend_heater_pwm
from ..realtime import socketio
from .pump_cycles import extract_pump_cycle_from_event
from .notification_email import notify_critical_alarms
from .telemetry_parser import TELEMETRY_COLUMNS

LOGGER = logging.getLogger(__name__)
SUBSCRIBED_SUFFIXES = ("telemetry", "state", "alarm", "heartbeat", "ota/status")
MAX_INT32 = 2_147_483_647
EVENT_NAMES = {
    1: "pump start",
    2: "pump end",
    3: "pump recovered",
    4: "hard kill",
    5: "sensor fault",
}


@dataclass
class TopicParts:
    site_id: str
    rig_id: str
    device_id: str
    message_type: str


class MqttRuntime:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.connected = False
        self.started = False
        self.last_message_at: str | None = None
        self.last_error = ""
        self.subscriptions: list[str] = []

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            return {
                "connected": self.connected,
                "started": self.started,
                "last_message_at": self.last_message_at,
                "last_error": self.last_error,
                "subscriptions": list(self.subscriptions),
            }


RUNTIME = MqttRuntime()
_CLIENT: mqtt.Client | None = None
_THREAD: threading.Thread | None = None


def topic_filter(base_topic: str, suffix: str) -> str:
    return f"{base_topic}/site/+/rig/+/device/+/{suffix}"


def parse_topic(topic: str, base_topic: str) -> TopicParts | None:
    parts = topic.split("/")
    base_parts = base_topic.split("/")
    if parts[: len(base_parts)] != base_parts:
        return None
    rest = parts[len(base_parts):]
    if len(rest) < 7:
        return None
    if rest[0] != "site" or rest[2] != "rig" or rest[4] != "device":
        return None
    message_type = "/".join(rest[6:])
    if message_type not in SUBSCRIBED_SUFFIXES:
        return None
    return TopicParts(site_id=rest[1], rig_id=rest[3], device_id=rest[5], message_type=message_type)


def bool_value(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    return str(value or "").strip().lower() in {"1", "true", "yes", "on", "online", "running", "enabled"}


def number_value(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def int_value(value: Any, default: int = 0) -> int:
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def bounded_int(value: Any, default: int = 0, upper: int = MAX_INT32) -> int:
    return max(0, min(int_value(value, default), upper))


def decode_payload(raw: bytes) -> tuple[dict[str, Any], bool, str]:
    try:
        decoded = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        return {}, False, f"Payload is not UTF-8: {exc}"
    try:
        parsed = json.loads(decoded or "{}")
    except json.JSONDecodeError as exc:
        return {}, False, f"Payload is not JSON: {exc}"
    if not isinstance(parsed, dict):
        return {}, False, "Payload must be a JSON object"
    return parsed, True, ""


def device_from_payload(parts: TopicParts, payload: dict[str, Any]) -> Device:
    now = datetime.utcnow()
    device = db.session.get(Device, parts.device_id) or Device(device_id=parts.device_id)
    previous_payload_at = device.last_payload_at
    device.site_id = str(payload.get("site_id") or parts.site_id)
    device.rig_id = str(payload.get("rig_id") or parts.rig_id)
    device.device_name = str(payload.get("device_name") or payload.get("name") or device.device_name or parts.device_id)
    device.firmware_version = str(payload.get("firmware_version") or device.firmware_version or "")
    device.ip_address = str(payload.get("ip_address") or payload.get("ip") or device.ip_address or "")
    device.mac_address = str(payload.get("mac_address") or payload.get("mac") or device.mac_address or "")
    status = payload.get("status") if isinstance(payload.get("status"), dict) else {}
    device.rssi_dbm = int_value(status.get("rssi_dbm"), device.rssi_dbm or 0)
    device.last_topic = f"{parts.site_id}/{parts.rig_id}/{parts.device_id}/{parts.message_type}"
    device.last_payload_at = now
    device.message_count = int(device.message_count or 0) + 1
    if previous_payload_at:
        delta = max(0.001, (now - previous_payload_at).total_seconds())
        device.message_rate_hz = round((device.message_rate_hz * 0.8) + ((1.0 / delta) * 0.2), 3)
    else:
        device.message_rate_hz = 0.0
    device.updated_at = now

    if parts.message_type in {"telemetry", "heartbeat"}:
        device.online = True
        device.last_heartbeat = now
    if parts.message_type == "state":
        device.state = str(payload.get("state") or payload.get("status") or "")
        if "online" in payload:
            device.online = bool_value(payload.get("online"))
        elif device.state:
            device.online = device.state.lower() not in {"offline", "disconnected"}
    return device


def normalize_telemetry(parts: TopicParts, payload: dict[str, Any], topic: str) -> dict[str, Any]:
    sensors = payload.get("sensors") if isinstance(payload.get("sensors"), dict) else {}
    status = payload.get("status") if isinstance(payload.get("status"), dict) else {}
    temp_c = number_value(
        payload.get("temp_c", sensors.get("block_temp_c", sensors.get("acid_outlet_temp_c", sensors.get("acid_inlet_temp_c")))),
        0.0,
    )
    pump_running = bool_value(payload.get("pump_on", payload.get("pump_enabled", status.get("pump_running"))))
    heater_enabled = bool_value(payload.get("heating", status.get("heater_enabled")))
    uptime_s = bounded_int(payload.get("uptime_s", status.get("uptime_s")), 0)
    ms_value = payload.get("ms", status.get("uptime_ms", uptime_s * 1000))
    row = {
        "site_id": str(payload.get("site_id") or parts.site_id),
        "rig_id": str(payload.get("rig_id") or parts.rig_id),
        "device_id": str(payload.get("device_id") or parts.device_id),
        "mqtt_topic": topic,
        "raw_payload": payload,
        "event": int_value(payload.get("event"), 0),
        "ms": bounded_int(ms_value),
        "temp_c": temp_c,
        "adc": int_value(payload.get("adc", sensors.get("adc")), 0),
        "dtemp_c_per_s": number_value(payload.get("dtemp_c_per_s"), 0.0),
        "setpoint_c": number_value(payload.get("setpoint_c"), temp_c),
        "mode": int_value(payload.get("mode"), 3 if heater_enabled else 0),
        "heater_pwm": int_value(payload.get("heater_pwm"), 255 if heater_enabled else 0),
        "heating": int_value(payload.get("heating"), 1 if heater_enabled else 0),
        "heater_lockout": int_value(payload.get("heater_lockout", status.get("heater_lockout")), 0),
        "pump_enabled": int_value(payload.get("pump_enabled"), 1 if pump_running else 0),
        "pump_allowed": int_value(payload.get("pump_allowed", status.get("pump_allowed")), 1),
        "pump_on": int_value(payload.get("pump_on"), 1 if pump_running else 0),
        "motor_pwm": int_value(payload.get("motor_pwm", sensors.get("pump_pwm")), 0),
        "motor_on_ms": int_value(payload.get("motor_on_ms"), 0),
        "motor_period_ms": int_value(payload.get("motor_period_ms"), 0),
        "temp_before_pump_c": number_value(payload.get("temp_before_pump_c"), temp_c),
        "min_temp_after_pump_c": number_value(payload.get("min_temp_after_pump_c"), temp_c),
        "last_pump_drop_c": number_value(payload.get("last_pump_drop_c"), 0.0),
        "recovery_time_s": number_value(payload.get("recovery_time_s"), 0.0),
        "manual_kill": int_value(payload.get("manual_kill", status.get("manual_kill")), 0),
        "hard_kill": int_value(payload.get("hard_kill", status.get("hard_kill")), 0),
        "uptime_s": uptime_s,
    }
    return {key: row.get(key, 0) for key in TELEMETRY_COLUMNS + ["site_id", "rig_id", "device_id", "mqtt_topic", "raw_payload"]}


def store_telemetry(row: dict[str, Any]) -> tuple[Telemetry, list[dict]]:
    telemetry = Telemetry(**row)
    db.session.add(telemetry)
    db.session.flush()
    event_code = int_value(row.get("event"), 0)
    if event_code != 0:
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
    for alarm in alarms:
        db.session.add(Alarm(telemetry_id=telemetry.id, **alarm))
    extract_pump_cycle_from_event(telemetry)
    return telemetry, alarms


def store_predictions_and_mpc(app: Flask, telemetry: Telemetry) -> None:
    predictor = ThermalPredictor(app.config["MODEL_DIR"])
    prediction_response = predictor.predict_all(telemetry.to_dict())
    for prediction in prediction_response.get("predictions", []):
        db.session.add(
            MlPredictionRecord(
                telemetry_id=telemetry.id,
                device_id=telemetry.device_id,
                horizon_s=int(prediction.get("horizon_s") or 0),
                current_temp_c=prediction.get("current_temp_c"),
                predicted_delta_c=prediction.get("predicted_delta_c"),
                predicted_temp_c=prediction.get("predicted_temp_c"),
                status=str(prediction.get("status") or "unavailable"),
                model_status=str(prediction.get("message") or ""),
            )
        )
    try:
        mpc = recommend_heater_pwm(telemetry.to_dict(), predictor=predictor)
    except Exception as exc:
        LOGGER.warning("MPC advisory skipped after MQTT ingest: %s", exc)
        return
    db.session.add(
        MpcRecommendation(
            current_temp_c=telemetry.temp_c,
            setpoint_c=telemetry.setpoint_c,
            current_pwm=telemetry.heater_pwm,
            recommended_pwm=mpc.recommended_pwm,
            predicted_temp_c=mpc.predicted_temp_c,
            cost=mpc.cost,
        )
    )


def log_message(topic: str, parts: TopicParts | None, payload: dict[str, Any], msg: mqtt.MQTTMessage, valid: bool, error: str) -> MqttMessageLog:
    message = MqttMessageLog(
        topic=topic,
        site_id=parts.site_id if parts else None,
        rig_id=parts.rig_id if parts else None,
        device_id=parts.device_id if parts else None,
        message_type=parts.message_type if parts else "unknown",
        qos=msg.qos,
        retained=bool(msg.retain),
        valid=valid,
        error=error or None,
        payload=payload if payload else None,
    )
    db.session.add(message)
    return message


def handle_message(app: Flask, msg: mqtt.MQTTMessage) -> None:
    topic = str(msg.topic)
    parts = parse_topic(topic, app.config["MQTT_BASE_TOPIC"])
    payload, valid, error = decode_payload(msg.payload)
    with app.app_context():
        log = log_message(topic, parts, payload, msg, valid and parts is not None, error if parts is not None else "Topic does not match SCC schema")
        device = None
        telemetry = None
        if valid and parts is not None:
            device = device_from_payload(parts, payload)
            db.session.add(device)
            if parts.message_type == "telemetry":
                telemetry_row = normalize_telemetry(parts, payload, topic)
                telemetry, telemetry_alarms = store_telemetry(telemetry_row)
                store_predictions_and_mpc(app, telemetry)
            elif parts.message_type == "alarm":
                device.alarm_status = str(payload.get("severity") or "warning")
                db.session.add(
                    Alarm(
                        severity=device.alarm_status,
                        alarm_code=str(payload.get("alarm_code") or "mqtt_alarm"),
                        message=str(payload.get("message") or "MQTT alarm"),
                    )
                )
            elif parts.message_type == "ota/status":
                db.session.add(
                    FirmwareUpdateEvent(
                        device_id=parts.device_id,
                        event_type="mqtt_ota_status",
                        ota_status=str(payload.get("ota_status") or payload.get("status") or "unknown"),
                        firmware_version=str(payload.get("firmware_version") or ""),
                        build_time=str(payload.get("build_time") or ""),
                        ip_address=str(payload.get("ip_address") or ""),
                        message=str(payload.get("message") or ""),
                        raw_payload=payload,
                    )
                )
        db.session.commit()
        if telemetry is not None:
            notify_critical_alarms(app, telemetry, telemetry_alarms)
        with RUNTIME.lock:
            RUNTIME.last_message_at = datetime.utcnow().isoformat()
            RUNTIME.last_error = error
        socketio.emit("mqtt_message", log.to_dict())
        if device is not None:
            socketio.emit("device_update", device.to_dict())
        if telemetry is not None:
            socketio.emit("telemetry_update", telemetry.to_dict())


def start_mqtt_subscriber(app: Flask) -> None:
    global _CLIENT, _THREAD
    if not app.config.get("MQTT_ENABLED", True):
        return
    with RUNTIME.lock:
        if RUNTIME.started:
            return
        RUNTIME.started = True

    base_topic = app.config["MQTT_BASE_TOPIC"]
    subscriptions = [topic_filter(base_topic, suffix) for suffix in SUBSCRIBED_SUFFIXES]
    with RUNTIME.lock:
        RUNTIME.subscriptions = subscriptions

    client = mqtt.Client(client_id=app.config["MQTT_CLIENT_ID"], clean_session=True)
    if app.config.get("MQTT_USERNAME"):
        client.username_pw_set(app.config["MQTT_USERNAME"], app.config.get("MQTT_PASSWORD") or None)

    def on_connect(client_obj: mqtt.Client, _userdata: Any, _flags: dict, rc: int) -> None:
        connected = rc == 0
        with RUNTIME.lock:
            RUNTIME.connected = connected
            RUNTIME.last_error = "" if connected else f"MQTT connect rc={rc}"
        if connected:
            for subscription in subscriptions:
                client_obj.subscribe(subscription, qos=1)
            LOGGER.info("MQTT subscriber connected and subscribed: %s", subscriptions)

    def on_disconnect(_client_obj: mqtt.Client, _userdata: Any, rc: int) -> None:
        with RUNTIME.lock:
            RUNTIME.connected = False
            RUNTIME.last_error = "" if rc == 0 else f"MQTT disconnected rc={rc}"

    def on_message(_client_obj: mqtt.Client, _userdata: Any, message: mqtt.MQTTMessage) -> None:
        try:
            handle_message(app, message)
        except Exception as exc:
            LOGGER.exception("MQTT message handling failed")
            with RUNTIME.lock:
                RUNTIME.last_error = str(exc)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    _CLIENT = client

    def run() -> None:
        while True:
            try:
                client.connect(app.config["MQTT_HOST"], int(app.config["MQTT_PORT"]), keepalive=30)
                client.loop_forever(retry_first_connection=True)
            except Exception as exc:
                LOGGER.exception("MQTT subscriber loop failed")
                with RUNTIME.lock:
                    RUNTIME.connected = False
                    RUNTIME.last_error = str(exc)
                threading.Event().wait(5)

    _THREAD = threading.Thread(target=run, name="mqtt-subscriber", daemon=True)
    _THREAD.start()


def mqtt_status() -> dict[str, Any]:
    return RUNTIME.snapshot()
