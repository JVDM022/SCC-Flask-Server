from __future__ import annotations

from io import BytesIO

import pytest

from app import create_app
from app.database.db import db
from app.database.models import ControlCommand, FirmwareArtifact, FirmwareCommand


class TestConfig:
    TESTING = True
    SQLALCHEMY_DATABASE_URI = "sqlite:///:memory:"
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    MODEL_DIR = "./models"
    CORS_ORIGINS = "*"
    FIRMWARE_SOURCE_DIR = "/app/firmware"
    FIRMWARE_PLATFORMIO_ENV = "esp32doit-devkit-v1"
    FIRMWARE_OTA_PORT = ""
    MQTT_ENABLED = False
    MQTT_HOST = "localhost"
    MQTT_PORT = 1883
    MQTT_CLIENT_ID = "test"
    MQTT_BASE_TOPIC = "scc"
    MQTT_USERNAME = ""
    MQTT_PASSWORD = ""
    PUBLIC_BASE_URL = "http://localhost:5000"


@pytest.fixture()
def client(tmp_path):
    class Config(TestConfig):
        FIRMWARE_STORAGE_DIR = str(tmp_path / "firmware")

    app = create_app(Config)
    with app.app_context():
        db.create_all()
    with app.test_client() as test_client:
        yield test_client
    with app.app_context():
        db.drop_all()


def upload(client, target: str, filename: str, data: bytes):
    return client.post(
        "/api/firmware/artifacts",
        data={"target": target, "file": (BytesIO(data), filename)},
        content_type="multipart/form-data",
    )


def test_esp32_accepts_bin(client):
    response = upload(client, "ESP32", "relay.bin", b"firmware")

    assert response.status_code == 201
    body = response.get_json()
    assert body["status"] == "ok"
    assert body["target"] == "ESP32"
    assert body["filename"].endswith(".bin")
    assert body["sizeBytes"] == len(b"firmware")
    assert len(body["sha256"]) == 64


def test_esp32_rejects_hex(client):
    response = upload(client, "ESP32", "relay.hex", b":00000001FF")

    assert response.status_code == 400
    assert ".bin" in response.get_json()["message"]


def test_arduino_accepts_hex(client):
    response = upload(client, "ARDUINO", "controller.hex", b":00000001FF")

    assert response.status_code == 201
    body = response.get_json()
    assert body["target"] == "ARDUINO"
    assert body["filename"].endswith(".hex")
    assert "Intel NUC" in body["notes"]


def test_firmware_command_queue_and_ack(client):
    upload_response = upload(client, "ESP32", "relay.bin", b"firmware")
    artifact_id = upload_response.get_json()["artifactId"]

    queue_response = client.post("/api/firmware/commands", json={"target": "ESP32", "artifactId": artifact_id})
    assert queue_response.status_code == 200
    queued = queue_response.get_json()["command"]
    assert queued["type"] == "OTA"
    assert queued["cmdId"] == 1

    next_response = client.get("/api/firmware/commands/next?device=esp32")
    assert next_response.status_code == 200
    assert next_response.get_json()["cmdId"] == 1

    ack_response = client.post("/api/firmware/commands/1/ack", json={"status": "success", "message": "done"})
    assert ack_response.status_code == 200
    assert ack_response.get_json()["command"]["status"] == "success"

    with client.application.app_context():
        assert FirmwareArtifact.query.count() == 1
        assert FirmwareCommand.query.first().ack_status == "success"


def test_nuc_only_fetches_arduino_firmware_commands(client):
    esp32_response = upload(client, "ESP32", "relay.bin", b"firmware")
    arduino_response = upload(client, "ARDUINO", "controller.hex", b":00000001FF")

    client.post("/api/firmware/commands", json={"target": "ESP32", "artifactId": esp32_response.get_json()["artifactId"]})
    client.post(
        "/api/firmware/commands",
        json={"target": "ARDUINO", "artifactId": arduino_response.get_json()["artifactId"]},
    )

    next_response = client.get("/api/firmware/commands/next?device=nuc")

    assert next_response.status_code == 200
    body = next_response.get_json()
    assert body["type"] == "ARDUINO_OTA"

    with client.application.app_context():
        assert FirmwareCommand.query.filter_by(target="ESP32").first().status == "pending"
        assert FirmwareCommand.query.filter_by(target="ARDUINO").first().status == "sent"


def test_nuc_fetches_setpoint_control_commands(client):
    with client.application.app_context():
        db.session.add(ControlCommand(command_type="SETPOINT", value=0, setpoint_c=124.5))
        db.session.commit()

    response = client.get("/api/firmware/commands/next?device=nuc")

    assert response.status_code == 200
    body = response.get_json()
    assert body["type"] == "SETPOINT"
    assert body["setpoint_c"] == 124.5


def test_nuc_fetches_power_control_commands(client):
    with client.application.app_context():
        db.session.add(ControlCommand(command_type="SET_ON", value=1, setpoint_c=0.0))
        db.session.commit()

    response = client.get("/api/firmware/commands/next?device=nuc")

    assert response.status_code == 200
    body = response.get_json()
    assert body["type"] == "SET_ON"
    assert body["value"] == 1
