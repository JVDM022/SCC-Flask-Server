from __future__ import annotations

import pytest

from app import create_app
from app.database.db import db
from app.database.models import Telemetry


ROW = "0,123456,126.42,222,-0.1300,125.00,3,184,1,0,1,1,0,155,150,30000,127.20,124.80,2.40,18.50,0,0,300"


class TestConfig:
    TESTING = True
    SQLALCHEMY_DATABASE_URI = "sqlite:///:memory:"
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    MODEL_DIR = "./models"
    CORS_ORIGINS = "*"
    API_WRITE_KEY = "test-write-key"
    FIRMWARE_SOURCE_DIR = "/app/firmware"
    FIRMWARE_PLATFORMIO_ENV = "esp32doit-devkit-v1"
    FIRMWARE_OTA_PORT = ""
    FIRMWARE_STORAGE_DIR = "/tmp/scc-test-firmware"
    MQTT_ENABLED = False
    MQTT_HOST = "localhost"
    MQTT_PORT = 1883
    MQTT_CLIENT_ID = "test"
    MQTT_BASE_TOPIC = "scc"
    MQTT_USERNAME = ""
    MQTT_PASSWORD = ""
    PUBLIC_BASE_URL = "http://localhost:5000"


@pytest.fixture()
def client():
    app = create_app(TestConfig)
    with app.app_context():
        db.create_all()
    with app.test_client() as test_client:
        yield test_client
    with app.app_context():
        db.drop_all()


def test_read_endpoint_does_not_require_api_key(client):
    response = client.get("/api/health")

    assert response.status_code == 200
    assert response.get_json()["status"] == "ok"


def test_telemetry_csv_rejects_missing_api_key(client):
    response = client.post("/api/telemetry/csv", data=ROW, content_type="text/plain")

    assert response.status_code == 401
    assert "API key" in response.get_json()["message"]


def test_telemetry_csv_accepts_x_api_key(client):
    response = client.post(
        "/api/telemetry/csv",
        data=ROW,
        content_type="text/plain",
        headers={"X-API-Key": "test-write-key"},
    )

    assert response.status_code == 200
    assert response.get_json()["rows_stored"] == 1


def test_telemetry_csv_accepts_bearer_api_key(client):
    response = client.post(
        "/api/telemetry/csv",
        data=ROW,
        content_type="text/plain",
        headers={"Authorization": "Bearer test-write-key"},
    )

    assert response.status_code == 200
    assert response.get_json()["rows_stored"] == 1


def test_telemetry_json_keeps_gateway_metadata(client):
    values = ROW.split(",")
    payload = {
        "event": int(values[0]),
        "ms": int(values[1]),
        "temp_c": float(values[2]),
        "adc": int(values[3]),
        "dtemp_c_per_s": float(values[4]),
        "setpoint_c": float(values[5]),
        "mode": int(values[6]),
        "heater_pwm": int(values[7]),
        "heating": int(values[8]),
        "heater_lockout": int(values[9]),
        "pump_enabled": int(values[10]),
        "pump_allowed": int(values[11]),
        "pump_on": int(values[12]),
        "motor_pwm": int(values[13]),
        "motor_on_ms": int(values[14]),
        "motor_period_ms": int(values[15]),
        "temp_before_pump_c": float(values[16]),
        "min_temp_after_pump_c": float(values[17]),
        "last_pump_drop_c": float(values[18]),
        "recovery_time_s": float(values[19]),
        "manual_kill": int(values[20]),
        "hard_kill": int(values[21]),
        "uptime_s": int(values[22]),
        "site_id": "site-01",
        "rig_id": "rig-01",
        "device_id": "intel-nuc-gateway",
        "raw_payload": {"source": "arduino_usb", "line": ROW},
    }

    response = client.post("/api/telemetry", json=payload, headers={"X-API-Key": "test-write-key"})

    assert response.status_code == 200
    with client.application.app_context():
        telemetry = Telemetry.query.first()
        assert telemetry.device_id == "intel-nuc-gateway"
        assert telemetry.site_id == "site-01"
        assert telemetry.raw_payload["source"] == "arduino_usb"


def test_telemetry_json_accepts_partial_payload(client):
    response = client.post(
        "/api/telemetry",
        json={"event": 0, "ms": 123456, "temp_c": 126.42},
        headers={"X-API-Key": "test-write-key"},
    )

    assert response.status_code == 200
    with client.application.app_context():
        telemetry = Telemetry.query.order_by(Telemetry.id.desc()).first()
        assert telemetry.temp_c == 126.42
        assert telemetry.adc is None
