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
    CORS_ORIGINS = os.getenv("CORS_ORIGINS", "*")
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
