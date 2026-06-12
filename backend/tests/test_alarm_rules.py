from __future__ import annotations

from app.alarms.rules import evaluate_alarms


def base_row(**overrides):
    row = {
        "temp_c": 126.0,
        "setpoint_c": 125.0,
        "adc": 222,
        "heater_lockout": 0,
        "manual_kill": 0,
        "hard_kill": 0,
        "recovery_time_s": 18.5,
        "last_pump_drop_c": 2.4,
    }
    row.update(overrides)
    return row


def test_safety_alarm_generation():
    alarms = evaluate_alarms(base_row(hard_kill=1, manual_kill=1, heater_lockout=1))
    codes = {alarm["alarm_code"] for alarm in alarms}

    assert "HARD_KILL" in codes
    assert "MANUAL_KILL" in codes
    assert "HEATER_LOCKOUT" in codes


def test_high_temperature_warning_and_critical():
    warning_codes = {alarm["alarm_code"] for alarm in evaluate_alarms(base_row(temp_c=136.0))}
    critical_codes = {alarm["alarm_code"] for alarm in evaluate_alarms(base_row(temp_c=146.0))}

    assert "HIGH_TEMP_WARNING" in warning_codes
    assert "HIGH_TEMP_CRITICAL" in critical_codes


def test_sensor_fault_for_missing_adc_or_temp():
    codes = {alarm["alarm_code"] for alarm in evaluate_alarms(base_row(adc=None))}

    assert "SENSOR_FAULT" in codes


def test_sensor_fault_for_firmware_event_code():
    codes = {alarm["alarm_code"] for alarm in evaluate_alarms(base_row(event=5))}

    assert "SENSOR_FAULT" in codes
