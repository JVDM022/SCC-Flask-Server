-- Arduino millis() is uint32 (0..4,294,967,295). PostgreSQL INTEGER is
-- signed int32 and fails after approximately 24.85 days of device uptime.
ALTER TABLE telemetry
    ALTER COLUMN ms TYPE BIGINT
    USING ms::BIGINT;
