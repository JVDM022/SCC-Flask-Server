CREATE TABLE IF NOT EXISTS telemetry (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    site_id VARCHAR(80),
    rig_id VARCHAR(120),
    device_id VARCHAR(120),
    mqtt_topic VARCHAR(255),
    raw_payload JSONB,
    event INTEGER NOT NULL,
    ms INTEGER NOT NULL,
    temp_c DOUBLE PRECISION NOT NULL,
    adc INTEGER NOT NULL,
    dtemp_c_per_s DOUBLE PRECISION NOT NULL,
    setpoint_c DOUBLE PRECISION NOT NULL,
    mode INTEGER NOT NULL,
    heater_pwm INTEGER NOT NULL,
    heating INTEGER NOT NULL,
    heater_lockout INTEGER NOT NULL,
    pump_enabled INTEGER NOT NULL,
    pump_allowed INTEGER NOT NULL,
    pump_on INTEGER NOT NULL,
    motor_pwm INTEGER NOT NULL,
    motor_on_ms INTEGER NOT NULL,
    motor_period_ms INTEGER NOT NULL,
    temp_before_pump_c DOUBLE PRECISION NOT NULL,
    min_temp_after_pump_c DOUBLE PRECISION NOT NULL,
    last_pump_drop_c DOUBLE PRECISION NOT NULL,
    recovery_time_s DOUBLE PRECISION NOT NULL,
    manual_kill INTEGER NOT NULL,
    hard_kill INTEGER NOT NULL,
    uptime_s INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS events (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    event_code INTEGER NOT NULL,
    event_name VARCHAR(80) NOT NULL,
    severity VARCHAR(32) NOT NULL,
    message VARCHAR(255) NOT NULL,
    telemetry_id INTEGER REFERENCES telemetry(id)
);

CREATE TABLE IF NOT EXISTS alarms (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    cleared_at TIMESTAMPTZ,
    active BOOLEAN NOT NULL DEFAULT TRUE,
    severity VARCHAR(32) NOT NULL,
    alarm_code VARCHAR(80) NOT NULL,
    message VARCHAR(255) NOT NULL,
    telemetry_id INTEGER REFERENCES telemetry(id)
);

CREATE TABLE IF NOT EXISTS pump_cycles (
    id SERIAL PRIMARY KEY,
    pump_start_time TIMESTAMPTZ,
    pump_end_time TIMESTAMPTZ,
    recovery_time TIMESTAMPTZ,
    temp_before_pump_c DOUBLE PRECISION,
    min_temp_after_pump_c DOUBLE PRECISION,
    last_pump_drop_c DOUBLE PRECISION,
    recovery_time_s DOUBLE PRECISION,
    avg_heater_pwm DOUBLE PRECISION,
    avg_motor_pwm DOUBLE PRECISION
);

CREATE TABLE IF NOT EXISTS model_metrics (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    model_name VARCHAR(120) NOT NULL,
    horizon_s INTEGER NOT NULL,
    mae DOUBLE PRECISION,
    rmse DOUBLE PRECISION,
    r2 DOUBLE PRECISION,
    within_0p5c_percent DOUBLE PRECISION,
    within_1c_percent DOUBLE PRECISION,
    within_2c_percent DOUBLE PRECISION
);

CREATE TABLE IF NOT EXISTS mpc_recommendations (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    current_temp_c DOUBLE PRECISION NOT NULL,
    setpoint_c DOUBLE PRECISION NOT NULL,
    current_pwm INTEGER NOT NULL,
    recommended_pwm INTEGER NOT NULL,
    predicted_temp_c DOUBLE PRECISION,
    cost DOUBLE PRECISION
);

CREATE TABLE IF NOT EXISTS control_commands (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    command_type VARCHAR(40) NOT NULL DEFAULT 'SETPOINT',
    value INTEGER NOT NULL DEFAULT 0,
    setpoint_c DOUBLE PRECISION NOT NULL,
    applied BOOLEAN NOT NULL DEFAULT FALSE,
    sent_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS devices (
    device_id VARCHAR(120) PRIMARY KEY,
    site_id VARCHAR(80) NOT NULL DEFAULT 'default',
    rig_id VARCHAR(120) NOT NULL DEFAULT 'default',
    device_name VARCHAR(160),
    firmware_version VARCHAR(80),
    ip_address VARCHAR(64),
    mac_address VARCHAR(64),
    rssi_dbm INTEGER,
    online BOOLEAN NOT NULL DEFAULT FALSE,
    state VARCHAR(80),
    last_topic VARCHAR(255),
    last_payload_at TIMESTAMPTZ,
    last_heartbeat TIMESTAMPTZ,
    message_count INTEGER NOT NULL DEFAULT 0,
    message_rate_hz DOUBLE PRECISION NOT NULL DEFAULT 0,
    alarm_status VARCHAR(32) NOT NULL DEFAULT 'normal',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS mqtt_message_log (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    topic VARCHAR(255) NOT NULL,
    site_id VARCHAR(80),
    rig_id VARCHAR(120),
    device_id VARCHAR(120),
    message_type VARCHAR(40) NOT NULL,
    qos INTEGER,
    retained BOOLEAN NOT NULL DEFAULT FALSE,
    valid BOOLEAN NOT NULL DEFAULT TRUE,
    error VARCHAR(255),
    payload JSONB
);

CREATE TABLE IF NOT EXISTS ml_predictions (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    telemetry_id INTEGER REFERENCES telemetry(id) ON DELETE SET NULL,
    device_id VARCHAR(120),
    horizon_s INTEGER NOT NULL,
    current_temp_c DOUBLE PRECISION,
    predicted_delta_c DOUBLE PRECISION,
    predicted_temp_c DOUBLE PRECISION,
    status VARCHAR(40) NOT NULL,
    model_status VARCHAR(255)
);

CREATE TABLE IF NOT EXISTS firmware_devices (
    device_id VARCHAR(120) PRIMARY KEY,
    device_name VARCHAR(160),
    ip_address VARCHAR(64),
    mac_address VARCHAR(64),
    firmware_version VARCHAR(80),
    build_time VARCHAR(120),
    platformio_env VARCHAR(120),
    firmware_source_dir VARCHAR(255),
    ota_port VARCHAR(24),
    ota_status VARCHAR(80) NOT NULL DEFAULT 'Idle',
    uptime INTEGER,
    online BOOLEAN NOT NULL DEFAULT FALSE,
    last_heartbeat TIMESTAMPTZ,
    last_ota_update_time TIMESTAMPTZ,
    raw_payload JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS firmware_update_events (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    device_id VARCHAR(120) REFERENCES firmware_devices(device_id) ON DELETE SET NULL,
    event_type VARCHAR(80) NOT NULL,
    ota_status VARCHAR(80) NOT NULL,
    firmware_version VARCHAR(80),
    build_time VARCHAR(120),
    ip_address VARCHAR(64),
    message VARCHAR(255),
    raw_payload JSONB
);

CREATE TABLE IF NOT EXISTS firmware_artifacts (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    target VARCHAR(24) NOT NULL,
    filename VARCHAR(255) NOT NULL UNIQUE,
    original_filename VARCHAR(255) NOT NULL,
    file_path VARCHAR(512) NOT NULL,
    url VARCHAR(512) NOT NULL,
    sha256 VARCHAR(64) NOT NULL,
    size_bytes INTEGER NOT NULL,
    uploaded_by VARCHAR(120),
    notes TEXT
);

CREATE TABLE IF NOT EXISTS firmware_commands (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    target VARCHAR(24) NOT NULL,
    command_type VARCHAR(40) NOT NULL,
    artifact_id INTEGER NOT NULL REFERENCES firmware_artifacts(id),
    command_json JSONB NOT NULL,
    status VARCHAR(24) NOT NULL DEFAULT 'pending',
    sent_at TIMESTAMPTZ,
    ack_at TIMESTAMPTZ,
    ack_status VARCHAR(24),
    ack_message VARCHAR(255)
);

ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS site_id VARCHAR(80);
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS rig_id VARCHAR(120);
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS device_id VARCHAR(120);
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS mqtt_topic VARCHAR(255);
ALTER TABLE telemetry ADD COLUMN IF NOT EXISTS raw_payload JSONB;
ALTER TABLE control_commands ADD COLUMN IF NOT EXISTS command_type VARCHAR(40) NOT NULL DEFAULT 'SETPOINT';
ALTER TABLE control_commands ADD COLUMN IF NOT EXISTS value INTEGER NOT NULL DEFAULT 0;
ALTER TABLE control_commands ADD COLUMN IF NOT EXISTS sent_at TIMESTAMPTZ;

CREATE INDEX IF NOT EXISTS idx_telemetry_created_at ON telemetry(created_at);
CREATE INDEX IF NOT EXISTS idx_telemetry_device_id ON telemetry(device_id);
CREATE INDEX IF NOT EXISTS idx_telemetry_site_rig ON telemetry(site_id, rig_id);
CREATE INDEX IF NOT EXISTS idx_events_created_at ON events(created_at);
CREATE INDEX IF NOT EXISTS idx_alarms_active ON alarms(active);
CREATE INDEX IF NOT EXISTS idx_firmware_devices_last_heartbeat ON firmware_devices(last_heartbeat);
CREATE INDEX IF NOT EXISTS idx_firmware_update_events_created_at ON firmware_update_events(created_at);
CREATE INDEX IF NOT EXISTS idx_firmware_artifacts_created_at ON firmware_artifacts(created_at);
CREATE INDEX IF NOT EXISTS idx_firmware_artifacts_target ON firmware_artifacts(target);
CREATE INDEX IF NOT EXISTS idx_firmware_commands_created_at ON firmware_commands(created_at);
CREATE INDEX IF NOT EXISTS idx_firmware_commands_status ON firmware_commands(status);
CREATE INDEX IF NOT EXISTS idx_firmware_commands_target ON firmware_commands(target);
CREATE INDEX IF NOT EXISTS idx_devices_last_heartbeat ON devices(last_heartbeat);
CREATE INDEX IF NOT EXISTS idx_mqtt_message_log_created_at ON mqtt_message_log(created_at);
CREATE INDEX IF NOT EXISTS idx_mqtt_message_log_device_id ON mqtt_message_log(device_id);
CREATE INDEX IF NOT EXISTS idx_ml_predictions_device_id ON ml_predictions(device_id);
CREATE INDEX IF NOT EXISTS idx_control_commands_applied ON control_commands(applied);
CREATE INDEX IF NOT EXISTS idx_control_commands_command_type ON control_commands(command_type);
