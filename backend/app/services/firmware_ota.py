from __future__ import annotations

from datetime import datetime, timedelta
from typing import Any

from flask import current_app

from ..database.db import db
from ..database.models import FirmwareDevice, FirmwareUpdateEvent

ONLINE_WINDOW_SECONDS = 45

STATUS_LABELS = {
    "idle": "Idle",
    "ota_started": "OTA started",
    "started": "OTA started",
    "flashing": "Flashing",
    "rebooting": "Rebooting",
    "reconnected": "Reconnected",
    "failed": "Failed",
    "version_verified": "Version verified",
    "complete": "Version verified",
    "completed": "Version verified",
}


def text_value(value: Any, default: str = "") -> str:
    if value is None:
        return default
    return str(value).strip() or default


def status_value(value: Any, default: str = "Idle") -> str:
    key = text_value(value).lower().replace("-", "_").replace(" ", "_")
    return STATUS_LABELS.get(key, text_value(value, default))


def int_value(value: Any) -> int | None:
    if value in (None, ""):
        return None
    try:
        result = int(float(value))
    except (TypeError, ValueError):
        return None
    return result if result >= 0 else None


def firmware_defaults() -> dict[str, str]:
    return {
        "platformio_env": current_app.config.get("FIRMWARE_PLATFORMIO_ENV", "esp32doit-devkit-v1"),
        "firmware_source_dir": current_app.config.get("FIRMWARE_SOURCE_DIR", "/app/firmware"),
        "ota_port": current_app.config.get("FIRMWARE_OTA_PORT", ""),
    }


def device_metadata(payload: dict[str, Any], existing: FirmwareDevice | None = None) -> dict[str, Any]:
    defaults = firmware_defaults()
    device_id = text_value(payload.get("device_id") or getattr(existing, "device_id", ""))
    mac_address = text_value(payload.get("mac_address") or payload.get("mac") or getattr(existing, "mac_address", ""))
    device_name = text_value(payload.get("device_name") or payload.get("name") or getattr(existing, "device_name", ""))
    if not device_name:
        device_name = device_id or f"ESP32 {mac_address[-5:]}" if mac_address else "ESP32"

    return {
        "device_id": device_id,
        "device_name": device_name,
        "ip_address": text_value(payload.get("ip_address") or payload.get("ip") or getattr(existing, "ip_address", "")),
        "mac_address": mac_address,
        "firmware_version": text_value(
            payload.get("firmware_version") or payload.get("version") or getattr(existing, "firmware_version", "")
        ),
        "build_time": text_value(payload.get("build_time") or payload.get("build_timestamp") or getattr(existing, "build_time", "")),
        "platformio_env": text_value(payload.get("platformio_env") or getattr(existing, "platformio_env", ""), defaults["platformio_env"]),
        "firmware_source_dir": text_value(
            payload.get("firmware_source_dir") or getattr(existing, "firmware_source_dir", ""), defaults["firmware_source_dir"]
        ),
        "ota_port": text_value(payload.get("ota_port") or getattr(existing, "ota_port", ""), defaults["ota_port"]),
        "ota_status": status_value(payload.get("ota_status") or getattr(existing, "ota_status", "")),
        "uptime": int_value(payload.get("uptime") or payload.get("uptime_seconds")),
        "raw_payload": payload,
    }


def require_device_id(data: dict[str, Any]) -> str:
    device_id = text_value(data.get("device_id"))
    if not device_id:
        raise ValueError("device_id is required")
    return device_id


def upsert_heartbeat(payload: dict[str, Any]) -> FirmwareDevice:
    data = device_metadata(payload)
    device_id = require_device_id(data)
    device = db.session.get(FirmwareDevice, device_id) or FirmwareDevice(device_id=device_id)

    for key, value in data.items():
        if key == "device_id":
            continue
        setattr(device, key, value)

    device.online = True
    device.last_heartbeat = datetime.utcnow()
    device.updated_at = datetime.utcnow()
    db.session.add(device)
    db.session.commit()
    return device


def record_event(payload: dict[str, Any], event_type: str, status: str) -> FirmwareDevice:
    requested = device_metadata(payload)
    device_id = require_device_id(requested)
    device = db.session.get(FirmwareDevice, device_id) or FirmwareDevice(device_id=device_id)
    data = device_metadata(payload, device)

    for key, value in data.items():
        if key == "device_id":
            continue
        if value not in ("", None) or key in {"ota_status", "raw_payload"}:
            setattr(device, key, value)

    device.ota_status = status_value(status)
    device.online = True
    device.updated_at = datetime.utcnow()
    if event_type in {"ota_complete", "ota_failed"}:
        device.last_ota_update_time = datetime.utcnow()

    db.session.add(device)
    db.session.flush()
    db.session.add(
        FirmwareUpdateEvent(
            device_id=device_id,
            event_type=event_type,
            ota_status=device.ota_status,
            firmware_version=device.firmware_version,
            build_time=device.build_time,
            ip_address=device.ip_address,
            message=text_value(payload.get("message") or payload.get("error")),
            raw_payload=payload,
        )
    )
    db.session.commit()
    return device


def device_to_dict(device: FirmwareDevice) -> dict[str, Any]:
    row = device.to_dict()
    online = bool(device.online)
    if device.last_heartbeat:
        online = online and datetime.utcnow() - device.last_heartbeat <= timedelta(seconds=ONLINE_WINDOW_SECONDS)

    upload_port = device.ip_address or "<device_ip>"
    if device.ip_address and device.ota_port:
        upload_port = f"{device.ip_address}:{device.ota_port}"

    row.update(
        {
            "online": online,
            "online_status": "Online" if online else "Offline",
            "last_known_ip": device.ip_address or "",
            "ota_command": f"pio run -t upload --upload-port {upload_port}",
        }
    )
    return row


def status_summary() -> dict[str, Any]:
    devices = FirmwareDevice.query.order_by(FirmwareDevice.updated_at.desc(), FirmwareDevice.device_id.asc()).all()
    events = FirmwareUpdateEvent.query.order_by(
        FirmwareUpdateEvent.created_at.desc(), FirmwareUpdateEvent.id.desc()
    ).limit(100).all()
    device_rows = [device_to_dict(device) for device in devices]
    defaults = firmware_defaults()
    return {
        "status": "ok",
        "platformio_env": defaults["platformio_env"],
        "firmware_source_dir": defaults["firmware_source_dir"],
        "ota_port": defaults["ota_port"],
        "device_count": len(device_rows),
        "online_count": sum(1 for device in device_rows if device["online"]),
        "devices": device_rows,
        "events": [event.to_dict() for event in events],
    }
