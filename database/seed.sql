INSERT INTO telemetry (
    event, ms, temp_c, adc, dtemp_c_per_s, setpoint_c, mode, heater_pwm,
    heating, heater_lockout, pump_enabled, pump_allowed, pump_on, motor_pwm,
    motor_on_ms, motor_period_ms, temp_before_pump_c, min_temp_after_pump_c,
    last_pump_drop_c, recovery_time_s, manual_kill, hard_kill, uptime_s
) VALUES (
    0, 123456, 126.42, 222, -0.1300, 125.00, 3, 184,
    1, 0, 1, 1, 0, 155,
    150, 30000, 127.20, 124.80,
    2.40, 18.50, 0, 0, 300
);
