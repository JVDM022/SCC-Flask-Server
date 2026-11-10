from __future__ import annotations

import math
from statistics import stdev
from typing import Any, Iterable


MIN_HISTORY_ROWS = 6


class FeatureHistoryError(ValueError):
    pass


def _number(row: dict[str, Any], key: str, default: float = 0.0) -> float:
    value = row.get(key)
    if value is None:
        return default
    try:
        number = float(value)
    except (TypeError, ValueError):
        return default
    return number if math.isfinite(number) else default


def build_feature_state(
    history: Iterable[dict[str, Any]],
    *,
    override_pwm: int | None = None,
) -> dict[str, float]:
    """Build the exact lag/rolling features used by the training notebook.

    ``history`` must be chronological and include the current telemetry row.
    The candidate PWM replaces the current row's observed heater PWM while
    earlier PWM samples remain unchanged.
    """

    rows = [dict(row) for row in history]
    if len(rows) < MIN_HISTORY_ROWS:
        raise FeatureHistoryError(
            f"At least {MIN_HISTORY_ROWS} chronological telemetry rows are required; got {len(rows)}."
        )

    current = rows[-1]
    current_pwm = _number(current, "heater_pwm") if override_pwm is None else float(override_pwm)

    def lag(key: str, count: int) -> float:
        return _number(rows[-1 - count], key)

    recent_five = rows[-5:]
    recent_ten = rows[-10:]
    temperatures = [_number(row, "temp_c") for row in recent_five]
    pwm_values = [_number(row, "heater_pwm") for row in recent_five]
    pwm_values[-1] = current_pwm

    event = int(_number(current, "event"))
    pump_on = _number(current, "pump_on")
    heater_lockout = _number(current, "heater_lockout")
    temp_c = _number(current, "temp_c")
    setpoint_c = _number(current, "setpoint_c", temp_c)
    heater_pwm_lag_1 = lag("heater_pwm", 1)

    return {
        "temp_c": temp_c,
        "heater_pwm": current_pwm,
        "pump_on": pump_on,
        "event": float(event),
        "motor_pwm": _number(current, "motor_pwm"),
        "dtemp_c_per_s": _number(current, "dtemp_c_per_s"),
        "setpoint_c": setpoint_c,
        "heater_lockout": heater_lockout,
        "temp_c_lag_1": lag("temp_c", 1),
        "temp_c_lag_2": lag("temp_c", 2),
        "temp_c_lag_5": lag("temp_c", 5),
        "heater_pwm_lag_1": heater_pwm_lag_1,
        "heater_pwm_lag_2": lag("heater_pwm", 2),
        "pump_on_lag_1": lag("pump_on", 1),
        "dtemp_c_per_s_lag_1": lag("dtemp_c_per_s", 1),
        "temp_mean_5": sum(temperatures) / len(temperatures),
        "temp_std_5": stdev(temperatures) if len(temperatures) > 1 else 0.0,
        "pwm_mean_5": sum(pwm_values) / len(pwm_values),
        "pump_recent_10": max(_number(row, "pump_on") for row in recent_ten),
        "temp_error": setpoint_c - temp_c,
        "pwm_delta": current_pwm - heater_pwm_lag_1,
        "pump_event_active": 1.0 if event in {1, 2, 3} else 0.0,
        "mode_normal": 1.0 if event == 0 and pump_on == 0 else 0.0,
        "mode_pump_start": 1.0 if event == 1 else 0.0,
        "mode_pump_running": 1.0 if pump_on == 1 else 0.0,
        "mode_recovery": 1.0 if event == 3 else 0.0,
        "mode_lockout": 1.0 if heater_lockout == 1 else 0.0,
    }
