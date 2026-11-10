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


class ReverseCausalityPredictor:
    def available_horizons(self):
        return [5]

    def predict(self, row, horizon_s, override_pwm=None, history=None):
        # Deliberately wrong learned relationship: lower PWM predicts more heat.
        return {
            "status": "ok",
            "horizon_s": horizon_s,
            "predicted_temp_c": float(row["temp_c"]) + ((200 - int(override_pwm)) / 20.0),
        }


def test_mpc_guardrail_requires_high_heat_when_far_below_setpoint():
    result = recommend_heater_pwm(
        base_row(temp_c=110.0, setpoint_c=125.0, heater_pwm=200),
        predictor=ReverseCausalityPredictor(),
        history=[base_row()] * 10,
    )

    assert result.recommended_pwm >= 184
