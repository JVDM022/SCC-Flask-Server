from __future__ import annotations

import hashlib
from datetime import datetime
from pathlib import Path
from typing import Any

from flask import Blueprint, current_app, jsonify, request, send_from_directory
from werkzeug.utils import secure_filename

from ..database.db import db
from ..database.models import ControlCommand, FirmwareArtifact, FirmwareCommand
from .auth import require_api_key

firmware_api = Blueprint("firmware_api", __name__)

TARGET_EXTENSIONS = {
    "ESP32": ".bin",
    "ARDUINO": ".hex",
}

COMMAND_TYPES = {
    "ESP32": "OTA",
    "ARDUINO": "ARDUINO_OTA",
}

COMMAND_STATUSES = {"pending", "sent", "started", "success", "failed", "cancelled"}
ACK_STATUSES = {"success", "failed", "started"}
CONTROL_CMD_ID_OFFSET = 100_000_000
RELAY_DEVICE_ALIASES = {"esp32", "relay"}
NUC_DEVICE_ALIASES = {"nuc", "intel-nuc", "intel_nuc", "nuc-gateway", "nuc_gateway"}


def normalize_target(value: Any) -> str:
    target = str(value or "").strip().upper()
    if target not in TARGET_EXTENSIONS:
        raise ValueError('target must be "ESP32" or "ARDUINO"')
    return target


def firmware_storage_dir() -> Path:
    return Path(current_app.config["FIRMWARE_STORAGE_DIR"]).expanduser().resolve()


def artifact_public_url(filename: str) -> str:
    return f"{current_app.config['PUBLIC_BASE_URL']}/firmware/artifacts/{filename}"


def artifact_to_response(artifact: FirmwareArtifact) -> dict[str, Any]:
    row = artifact.to_dict()
    row["artifactId"] = artifact.id
    row["sizeBytes"] = artifact.size_bytes
    return row


def command_to_response(command: FirmwareCommand) -> dict[str, Any]:
    row = command.to_dict()
    row["cmdId"] = command.id
    return row


def control_command_payload(command: ControlCommand) -> dict[str, Any]:
    cmd_id = CONTROL_CMD_ID_OFFSET + command.id
    if command.command_type == "KILL":
        return {"cmdId": cmd_id, "type": "KILL", "value": command.value}
    if command.command_type == "SETPOINT":
        return {"cmdId": cmd_id, "type": "SETPOINT", "setpoint_c": command.setpoint_c}
    return {"cmdId": cmd_id, "type": command.command_type, "value": command.value}


@firmware_api.post("/api/firmware/artifacts")
@require_api_key
def upload_firmware_artifact():
    # This endpoint should not be exposed publicly without authentication and role checks.
    try:
        target = normalize_target(request.form.get("target"))
    except ValueError as exc:
        return jsonify({"status": "error", "message": str(exc)}), 400

    upload = request.files.get("file")
    if upload is None or not upload.filename:
        return jsonify({"status": "error", "message": "file is required"}), 400

    original_filename = upload.filename
    safe_original = secure_filename(original_filename)
    if not safe_original:
        return jsonify({"status": "error", "message": "filename is invalid"}), 400

    extension = Path(safe_original).suffix.lower()
    expected_extension = TARGET_EXTENSIONS[target]
    if extension != expected_extension:
        return jsonify({"status": "error", "message": f"{target} firmware must use {expected_extension} files"}), 400

    file_bytes = upload.read()
    if not file_bytes:
        return jsonify({"status": "error", "message": "firmware artifact is empty"}), 400

    digest = hashlib.sha256(file_bytes).hexdigest()
    timestamp = datetime.utcnow().strftime("%Y_%m_%d_%H%M%S")
    filename = f"{target.lower()}_{timestamp}_{digest[:12]}{extension}"
    storage_dir = firmware_storage_dir()
    storage_dir.mkdir(parents=True, exist_ok=True)
    file_path = storage_dir / filename
    file_path.write_bytes(file_bytes)

    notes = None
    if target == "ARDUINO":
        notes = "Arduino .hex uploads are flashed by the Intel NUC USB gateway when it polls ARDUINO_OTA commands."

    artifact = FirmwareArtifact(
        target=target,
        filename=filename,
        original_filename=safe_original,
        file_path=str(file_path),
        url=artifact_public_url(filename),
        sha256=digest,
        size_bytes=len(file_bytes),
        uploaded_by=request.form.get("uploaded_by") or None,
        notes=notes,
    )
    db.session.add(artifact)
    db.session.commit()

    response = artifact_to_response(artifact)
    response.update(
        {
            "status": "ok",
            "target": artifact.target,
            "filename": artifact.filename,
            "url": artifact.url,
            "sha256": artifact.sha256,
            "sizeBytes": artifact.size_bytes,
        }
    )
    return jsonify(response), 201


@firmware_api.get("/api/firmware/artifacts")
def list_firmware_artifacts():
    limit = request.args.get("limit", default=50, type=int)
    rows = FirmwareArtifact.query.order_by(FirmwareArtifact.created_at.desc(), FirmwareArtifact.id.desc()).limit(max(1, limit)).all()
    return jsonify({"artifacts": [artifact_to_response(row) for row in rows]})


@firmware_api.post("/api/firmware/commands")
@require_api_key
def create_firmware_command():
    payload = request.get_json(silent=True) or {}
    try:
        target = normalize_target(payload.get("target"))
    except ValueError as exc:
        return jsonify({"status": "error", "message": str(exc)}), 400

    artifact_id = payload.get("artifactId") or payload.get("artifact_id")
    try:
        artifact_id = int(artifact_id)
    except (TypeError, ValueError):
        return jsonify({"status": "error", "message": "artifactId must be provided as an integer"}), 400

    artifact = db.session.get(FirmwareArtifact, artifact_id)
    if artifact is None:
        return jsonify({"status": "error", "message": "artifact was not found"}), 404
    if artifact.target != target:
        return jsonify({"status": "error", "message": "artifact target does not match command target"}), 400

    command_type = COMMAND_TYPES[target]
    command_json: dict[str, Any] = {
        "type": command_type,
        "url": artifact.url,
    }
    if target == "ARDUINO":
        command_json["note"] = "Arduino OTA command queued for the Intel NUC USB gateway."

    command = FirmwareCommand(
        target=target,
        command_type=command_type,
        artifact_id=artifact.id,
        command_json=command_json,
        status="pending",
    )
    db.session.add(command)
    db.session.flush()
    command.command_json = {"cmdId": command.id, **command_json}
    db.session.commit()

    return jsonify({"status": "queued", "command": command.command_json})


@firmware_api.get("/api/firmware/commands")
def list_firmware_commands():
    limit = request.args.get("limit", default=50, type=int)
    rows = FirmwareCommand.query.order_by(FirmwareCommand.created_at.desc(), FirmwareCommand.id.desc()).limit(max(1, limit)).all()
    return jsonify({"commands": [command_to_response(row) for row in rows]})


@firmware_api.get("/api/firmware/commands/next")
def get_next_firmware_command():
    device = str(request.args.get("device") or "").strip().lower()
    if device and device not in RELAY_DEVICE_ALIASES | NUC_DEVICE_ALIASES:
        return jsonify({"status": "none"})

    control_command = (
        ControlCommand.query.filter_by(applied=False)
        .filter(ControlCommand.command_type.in_(["KILL", "SETPOINT"] if device in NUC_DEVICE_ALIASES else ["KILL"]))
        .order_by(ControlCommand.created_at.asc(), ControlCommand.id.asc())
        .first()
    )
    if control_command is not None:
        control_command.applied = True
        control_command.sent_at = datetime.utcnow()
        db.session.commit()
        return jsonify(control_command_payload(control_command))

    query = FirmwareCommand.query.filter_by(status="pending")
    if device in NUC_DEVICE_ALIASES:
        query = query.filter_by(target="ARDUINO")
    command = query.order_by(FirmwareCommand.created_at.asc(), FirmwareCommand.id.asc()).first()
    if command is None:
        return jsonify({"status": "none"})

    # Mark-on-fetch prevents repeated polling from re-running the same OTA command.
    # Devices should acknowledge started/success/failed through the ack endpoint.
    command.status = "sent"
    command.sent_at = datetime.utcnow()
    command.updated_at = datetime.utcnow()
    db.session.commit()
    return jsonify(command.command_json)


@firmware_api.post("/api/firmware/commands/<int:cmd_id>/ack")
@require_api_key
def ack_firmware_command(cmd_id: int):
    command = db.session.get(FirmwareCommand, cmd_id)
    if command is None:
        return jsonify({"status": "error", "message": "command was not found"}), 404

    payload = request.get_json(silent=True) or {}
    ack_status = str(payload.get("status") or "").strip().lower()
    if ack_status not in ACK_STATUSES:
        return jsonify({"status": "error", "message": 'status must be "success", "failed", or "started"'}), 400

    command.status = ack_status
    command.ack_status = ack_status
    command.ack_message = str(payload.get("message") or "").strip()[:255] or None
    command.ack_at = datetime.utcnow()
    command.updated_at = datetime.utcnow()
    db.session.commit()
    return jsonify({"status": "ok", "command": command_to_response(command)})


@firmware_api.get("/firmware/artifacts/<path:filename>")
def serve_firmware_artifact(filename: str):
    safe_name = secure_filename(filename)
    if safe_name != filename:
        return jsonify({"status": "error", "message": "filename is invalid"}), 400
    return send_from_directory(firmware_storage_dir(), safe_name, as_attachment=False)
