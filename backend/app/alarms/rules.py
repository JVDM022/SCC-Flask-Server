from __future__ import annotations


SLOW_RECOVERY_SECONDS = 60.0
EXCESSIVE_PUMP_DROP_C = 5.0


def evaluate_alarms(row: dict) -> list[dict]:
    alarms: list[dict] = []

    def add(severity: str, alarm_code: str, message: str) -> None:
        alarms.append({"severity": severity, "alarm_code": alarm_code, "message": message})

    if row.get("hard_kill") == 1:
        add("critical", "HARD_KILL", "ESP32 hard kill is active.")
    if row.get("manual_kill") == 1:
        add("critical", "MANUAL_KILL", "Manual kill switch is active.")
    if row.get("heater_lockout") == 1:
        add("warning", "HEATER_LOCKOUT", "Heater lockout is active.")
    if row.get("event") == 5:
        add("critical", "SENSOR_FAULT", "NTC/ADC sensor fault is active.")

    temp_c = row.get("temp_c")
    setpoint_c = row.get("setpoint_c")
    adc = row.get("adc")
    if temp_c is None or adc is None:
        add("critical", "SENSOR_FAULT", "Temperature or ADC reading is missing.")
    elif setpoint_c is not None:
        if temp_c > setpoint_c + 20:
            add("critical", "HIGH_TEMP_CRITICAL", "Temperature exceeds setpoint by more than 20 C.")
        elif temp_c > setpoint_c + 10:
            add("warning", "HIGH_TEMP_WARNING", "Temperature exceeds setpoint by more than 10 C.")

    recovery_time_s = row.get("recovery_time_s")
    if recovery_time_s is not None and recovery_time_s > SLOW_RECOVERY_SECONDS:
        add("warning", "SLOW_RECOVERY", "Pump recovery time is above the configured threshold.")

    last_pump_drop_c = row.get("last_pump_drop_c")
    if last_pump_drop_c is not None and last_pump_drop_c > EXCESSIVE_PUMP_DROP_C:
        add("warning", "EXCESSIVE_PUMP_COOLING", "Pump cooling drop is above the configured threshold.")

    return alarms
