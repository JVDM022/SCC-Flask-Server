from __future__ import annotations

import os
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parents[2]


class Config:
    SQLALCHEMY_DATABASE_URI = os.getenv(
        "DATABASE_URL",
        "postgresql+psycopg2://scc:scc@localhost:5432/scc_control",
    )
    if SQLALCHEMY_DATABASE_URI.startswith("postgres://"):
        SQLALCHEMY_DATABASE_URI = SQLALCHEMY_DATABASE_URI.replace("postgres://", "postgresql://", 1)

    SQLALCHEMY_TRACK_MODIFICATIONS = False
    MODEL_DIR = os.getenv("MODEL_DIR", str(BASE_DIR.parent / "models"))
    MPC_CLOSED_LOOP_ENABLED = os.getenv("MPC_CLOSED_LOOP_ENABLED", "0") == "1"
    MPC_MAX_PWM = min(200, max(0, int(os.getenv("MPC_MAX_PWM", "200"))))
    MPC_MAX_SAMPLE_GAP_SECONDS = float(os.getenv("MPC_MAX_SAMPLE_GAP_SECONDS", "3.0"))
    MPC_PREHEAT_MAX_C = float(os.getenv("MPC_PREHEAT_MAX_C", "128.0"))
    MPC_PUMP_PROBE_PWM = min(255, max(0, int(os.getenv("MPC_PUMP_PROBE_PWM", "155"))))
    # Must match firmware/arduino/SCC-V1.4/include/config.h.
    MPC_FIRMWARE_WATCHDOG_MS = 3500
    CORS_ORIGINS = os.getenv("CORS_ORIGINS", "*")
    API_WRITE_KEY = os.getenv("API_WRITE_KEY", "")
    FIRMWARE_SOURCE_DIR = os.getenv("FIRMWARE_SOURCE_DIR", "/app/firmware")
    FIRMWARE_PLATFORMIO_ENV = os.getenv("FIRMWARE_PLATFORMIO_ENV", "esp32doit-devkit-v1")
    FIRMWARE_OTA_PORT = os.getenv("FIRMWARE_OTA_PORT", "")
    FIRMWARE_STORAGE_DIR = os.getenv("FIRMWARE_STORAGE_DIR", str(BASE_DIR / "storage" / "firmware"))
    PUBLIC_BASE_URL = os.getenv("PUBLIC_BASE_URL", "http://localhost:5000").rstrip("/")
    MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
    MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
    MQTT_CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "scc-backend")
    MQTT_BASE_TOPIC = os.getenv("MQTT_BASE_TOPIC", "scc").strip("/")
    MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
    MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
    MQTT_ENABLED = os.getenv("MQTT_ENABLED", "1") == "1"
    ALERT_EMAIL_ENABLED = os.getenv("ALERT_EMAIL_ENABLED", "0") == "1"
    ALERT_EMAIL_TO = os.getenv("ALERT_EMAIL_TO", "")
    ALERT_EMAIL_FROM = os.getenv("ALERT_EMAIL_FROM", "")
    ALERT_EMAIL_COOLDOWN_SECONDS = int(os.getenv("ALERT_EMAIL_COOLDOWN_SECONDS", "900"))
    SMTP_HOST = os.getenv("SMTP_HOST", "")
    SMTP_PORT = int(os.getenv("SMTP_PORT", "587"))
    SMTP_USERNAME = os.getenv("SMTP_USERNAME", "")
    SMTP_PASSWORD = os.getenv("SMTP_PASSWORD", "")
    SMTP_STARTTLS = os.getenv("SMTP_STARTTLS", "1") == "1"
