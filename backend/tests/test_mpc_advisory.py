from __future__ import annotations

from app.mpc.advisory import recommend_heater_pwm


def base_row(**overrides):
    row = {
        "temp_c": 126.0,
        "dtemp_c_per_s": -0.13,
        "setpoint_c": 125.0,
        "heater_pwm": 184,
        "hard_kill": 0,
        "manual_kill": 0,
        "heater_lockout": 0,
    }
    row.update(overrides)
    return row


def test_mpc_returns_zero_during_hard_kill():
    result = recommend_heater_pwm(base_row(hard_kill=1))

    assert result.recommended_pwm == 0
    assert result.status == "safety_override"


def test_mpc_returns_zero_during_manual_kill():
    result = recommend_heater_pwm(base_row(manual_kill=1))

    assert result.recommended_pwm == 0
    assert result.status == "safety_override"


def test_mpc_returns_zero_during_heater_lockout():
    result = recommend_heater_pwm(base_row(heater_lockout=1))

    assert result.recommended_pwm == 0
    assert result.status == "safety_override"
