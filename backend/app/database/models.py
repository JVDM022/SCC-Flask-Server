from __future__ import annotations

from datetime import datetime

from .db import db


class SerializerMixin:
    def to_dict(self) -> dict:
        data = {}
        for column in self.__table__.columns:
            value = getattr(self, column.name)
            if isinstance(value, datetime):
                value = value.isoformat()
            data[column.name] = value
        return data


class Telemetry(db.Model, SerializerMixin):
    __tablename__ = "telemetry"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    site_id = db.Column(db.String(80), nullable=True, index=True)
    rig_id = db.Column(db.String(120), nullable=True, index=True)
    device_id = db.Column(db.String(120), nullable=True, index=True)
    mqtt_topic = db.Column(db.String(255), nullable=True)
    raw_payload = db.Column(db.JSON, nullable=True)
    event = db.Column(db.Integer, nullable=False)
    ms = db.Column(db.Integer, nullable=False)
    temp_c = db.Column(db.Float, nullable=False)
    adc = db.Column(db.Integer, nullable=False)
    dtemp_c_per_s = db.Column(db.Float, nullable=False)
    setpoint_c = db.Column(db.Float, nullable=False)
    mode = db.Column(db.Integer, nullable=False)
    heater_pwm = db.Column(db.Integer, nullable=False)
    heating = db.Column(db.Integer, nullable=False)
    heater_lockout = db.Column(db.Integer, nullable=False)
    pump_enabled = db.Column(db.Integer, nullable=False)
    pump_allowed = db.Column(db.Integer, nullable=False)
    pump_on = db.Column(db.Integer, nullable=False)
    motor_pwm = db.Column(db.Integer, nullable=False)
    motor_on_ms = db.Column(db.Integer, nullable=False)
    motor_period_ms = db.Column(db.Integer, nullable=False)
    temp_before_pump_c = db.Column(db.Float, nullable=False)
    min_temp_after_pump_c = db.Column(db.Float, nullable=False)
    last_pump_drop_c = db.Column(db.Float, nullable=False)
    recovery_time_s = db.Column(db.Float, nullable=False)
    manual_kill = db.Column(db.Integer, nullable=False)
    hard_kill = db.Column(db.Integer, nullable=False)
    uptime_s = db.Column(db.Integer, nullable=False)


class Event(db.Model, SerializerMixin):
    __tablename__ = "events"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    event_code = db.Column(db.Integer, nullable=False)
    event_name = db.Column(db.String(80), nullable=False)
    severity = db.Column(db.String(32), nullable=False)
    message = db.Column(db.String(255), nullable=False)
    telemetry_id = db.Column(db.Integer, db.ForeignKey("telemetry.id"), nullable=True)


class Alarm(db.Model, SerializerMixin):
    __tablename__ = "alarms"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    cleared_at = db.Column(db.DateTime, nullable=True)
    active = db.Column(db.Boolean, default=True, nullable=False)
    severity = db.Column(db.String(32), nullable=False)
    alarm_code = db.Column(db.String(80), nullable=False)
    message = db.Column(db.String(255), nullable=False)
    telemetry_id = db.Column(db.Integer, db.ForeignKey("telemetry.id"), nullable=True)


class PumpCycle(db.Model, SerializerMixin):
    __tablename__ = "pump_cycles"

    id = db.Column(db.Integer, primary_key=True)
    pump_start_time = db.Column(db.DateTime, nullable=True)
    pump_end_time = db.Column(db.DateTime, nullable=True)
    recovery_time = db.Column(db.DateTime, nullable=True)
    temp_before_pump_c = db.Column(db.Float, nullable=True)
    min_temp_after_pump_c = db.Column(db.Float, nullable=True)
    last_pump_drop_c = db.Column(db.Float, nullable=True)
    recovery_time_s = db.Column(db.Float, nullable=True)
    avg_heater_pwm = db.Column(db.Float, nullable=True)
    avg_motor_pwm = db.Column(db.Float, nullable=True)


class ModelMetric(db.Model, SerializerMixin):
    __tablename__ = "model_metrics"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False)
    model_name = db.Column(db.String(120), nullable=False)
    horizon_s = db.Column(db.Integer, nullable=False)
    mae = db.Column(db.Float, nullable=True)
    rmse = db.Column(db.Float, nullable=True)
    r2 = db.Column(db.Float, nullable=True)
    within_0p5c_percent = db.Column(db.Float, nullable=True)
    within_1c_percent = db.Column(db.Float, nullable=True)
    within_2c_percent = db.Column(db.Float, nullable=True)


class MpcRecommendation(db.Model, SerializerMixin):
    __tablename__ = "mpc_recommendations"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    current_temp_c = db.Column(db.Float, nullable=False)
    setpoint_c = db.Column(db.Float, nullable=False)
    current_pwm = db.Column(db.Integer, nullable=False)
    recommended_pwm = db.Column(db.Integer, nullable=False)
    predicted_temp_c = db.Column(db.Float, nullable=True)
    cost = db.Column(db.Float, nullable=True)


class ControlCommand(db.Model, SerializerMixin):
    __tablename__ = "control_commands"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    command_type = db.Column(db.String(40), nullable=False, default="SETPOINT", index=True)
    value = db.Column(db.Integer, nullable=False, default=0)
    setpoint_c = db.Column(db.Float, nullable=False)
    applied = db.Column(db.Boolean, default=False, nullable=False)
    sent_at = db.Column(db.DateTime, nullable=True)


class Device(db.Model, SerializerMixin):
    __tablename__ = "devices"

    device_id = db.Column(db.String(120), primary_key=True)
    site_id = db.Column(db.String(80), nullable=False, default="default", index=True)
    rig_id = db.Column(db.String(120), nullable=False, default="default", index=True)
    device_name = db.Column(db.String(160), nullable=True)
    firmware_version = db.Column(db.String(80), nullable=True)
    ip_address = db.Column(db.String(64), nullable=True)
    mac_address = db.Column(db.String(64), nullable=True)
    rssi_dbm = db.Column(db.Integer, nullable=True)
    online = db.Column(db.Boolean, nullable=False, default=False)
    state = db.Column(db.String(80), nullable=True)
    last_topic = db.Column(db.String(255), nullable=True)
    last_payload_at = db.Column(db.DateTime, nullable=True)
    last_heartbeat = db.Column(db.DateTime, nullable=True, index=True)
    message_count = db.Column(db.Integer, nullable=False, default=0)
    message_rate_hz = db.Column(db.Float, nullable=False, default=0.0)
    alarm_status = db.Column(db.String(32), nullable=False, default="normal")
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False)


class MqttMessageLog(db.Model, SerializerMixin):
    __tablename__ = "mqtt_message_log"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    topic = db.Column(db.String(255), nullable=False, index=True)
    site_id = db.Column(db.String(80), nullable=True)
    rig_id = db.Column(db.String(120), nullable=True)
    device_id = db.Column(db.String(120), nullable=True, index=True)
    message_type = db.Column(db.String(40), nullable=False)
    qos = db.Column(db.Integer, nullable=True)
    retained = db.Column(db.Boolean, nullable=False, default=False)
    valid = db.Column(db.Boolean, nullable=False, default=True)
    error = db.Column(db.String(255), nullable=True)
    payload = db.Column(db.JSON, nullable=True)


class MlPredictionRecord(db.Model, SerializerMixin):
    __tablename__ = "ml_predictions"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    telemetry_id = db.Column(db.Integer, db.ForeignKey("telemetry.id"), nullable=True)
    device_id = db.Column(db.String(120), nullable=True, index=True)
    horizon_s = db.Column(db.Integer, nullable=False)
    current_temp_c = db.Column(db.Float, nullable=True)
    predicted_delta_c = db.Column(db.Float, nullable=True)
    predicted_temp_c = db.Column(db.Float, nullable=True)
    status = db.Column(db.String(40), nullable=False)
    model_status = db.Column(db.String(255), nullable=True)


class FirmwareDevice(db.Model, SerializerMixin):
    __tablename__ = "firmware_devices"

    device_id = db.Column(db.String(120), primary_key=True)
    device_name = db.Column(db.String(160), nullable=True)
    ip_address = db.Column(db.String(64), nullable=True)
    mac_address = db.Column(db.String(64), nullable=True)
    firmware_version = db.Column(db.String(80), nullable=True)
    build_time = db.Column(db.String(120), nullable=True)
    platformio_env = db.Column(db.String(120), nullable=True)
    firmware_source_dir = db.Column(db.String(255), nullable=True)
    ota_port = db.Column(db.String(24), nullable=True)
    ota_status = db.Column(db.String(80), nullable=False, default="Idle")
    uptime = db.Column(db.Integer, nullable=True)
    online = db.Column(db.Boolean, nullable=False, default=False)
    last_heartbeat = db.Column(db.DateTime, nullable=True, index=True)
    last_ota_update_time = db.Column(db.DateTime, nullable=True)
    raw_payload = db.Column(db.JSON, nullable=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False)


class FirmwareUpdateEvent(db.Model, SerializerMixin):
    __tablename__ = "firmware_update_events"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    device_id = db.Column(db.String(120), db.ForeignKey("firmware_devices.device_id"), nullable=True)
    event_type = db.Column(db.String(80), nullable=False)
    ota_status = db.Column(db.String(80), nullable=False)
    firmware_version = db.Column(db.String(80), nullable=True)
    build_time = db.Column(db.String(120), nullable=True)
    ip_address = db.Column(db.String(64), nullable=True)
    message = db.Column(db.String(255), nullable=True)
    raw_payload = db.Column(db.JSON, nullable=True)


class FirmwareArtifact(db.Model, SerializerMixin):
    __tablename__ = "firmware_artifacts"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    target = db.Column(db.String(24), nullable=False, index=True)
    filename = db.Column(db.String(255), nullable=False, unique=True)
    original_filename = db.Column(db.String(255), nullable=False)
    file_path = db.Column(db.String(512), nullable=False)
    url = db.Column(db.String(512), nullable=False)
    sha256 = db.Column(db.String(64), nullable=False)
    size_bytes = db.Column(db.Integer, nullable=False)
    uploaded_by = db.Column(db.String(120), nullable=True)
    notes = db.Column(db.Text, nullable=True)


class FirmwareCommand(db.Model, SerializerMixin):
    __tablename__ = "firmware_commands"

    id = db.Column(db.Integer, primary_key=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False, index=True)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False)
    target = db.Column(db.String(24), nullable=False, index=True)
    command_type = db.Column(db.String(40), nullable=False, index=True)
    artifact_id = db.Column(db.Integer, db.ForeignKey("firmware_artifacts.id"), nullable=False)
    command_json = db.Column(db.JSON, nullable=False)
    status = db.Column(db.String(24), nullable=False, default="pending", index=True)
    sent_at = db.Column(db.DateTime, nullable=True)
    ack_at = db.Column(db.DateTime, nullable=True)
    ack_status = db.Column(db.String(24), nullable=True)
    ack_message = db.Column(db.String(255), nullable=True)
