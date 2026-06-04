from __future__ import annotations

from dataclasses import dataclass


candidate_pwm_values = [0, 64, 128, 184, 220, 255]


@dataclass(frozen=True)
class MpcResult:
    status: str
    current_pwm: int
    recommended_pwm: int
    predicted_temp_c: float | None
    setpoint_c: float
    cost: float | None
    model_status: str
    advisory_only: bool = True

    def to_dict(self) -> dict:
        return self.__dict__.copy()


def _clamp_pwm(value: int) -> int:
    return max(0, min(255, int(value)))


def _fallback_predict_temp(row: dict, candidate_pwm: int) -> float:
    current_temp = float(row.get("temp_c", 0.0) or 0.0)
    dtemp = float(row.get("dtemp_c_per_s", 0.0) or 0.0)
    pwm_delta = candidate_pwm - int(row.get("heater_pwm", 0) or 0)
    return current_temp + (5.0 * dtemp) + (0.002 * pwm_delta)


def recommend_heater_pwm(row: dict, predictor=None) -> MpcResult:
    current_pwm = _clamp_pwm(int(row.get("heater_pwm", 0) or 0))
    setpoint_c = float(row.get("setpoint_c", 0.0) or 0.0)

    if row.get("hard_kill") == 1 or row.get("manual_kill") == 1 or row.get("heater_lockout") == 1:
        return MpcResult(
            status="safety_override",
            current_pwm=current_pwm,
            recommended_pwm=0,
            predicted_temp_c=None,
            setpoint_c=setpoint_c,
            cost=0.0,
            model_status="bypassed_by_safety",
        )

    best_pwm = 0
    best_temp: float | None = None
    best_cost: float | None = None
    model_status = "fallback"

    for candidate in candidate_pwm_values:
        candidate = _clamp_pwm(candidate)
        if predictor is not None and 5 in predictor.available_horizons():
            prediction = predictor.predict(row, 5, override_pwm=candidate)
            predicted_temp = float(prediction["predicted_temp_c"])
            model_status = "ok"
        else:
            predicted_temp = _fallback_predict_temp(row, candidate)

        cost = ((predicted_temp - setpoint_c) ** 2) + (0.0005 * candidate**2) + (
            0.05 * (candidate - current_pwm) ** 2
        )
        if best_cost is None or cost < best_cost:
            best_pwm = candidate
            best_temp = predicted_temp
            best_cost = cost

    return MpcResult(
        status="ok",
        current_pwm=current_pwm,
        recommended_pwm=_clamp_pwm(best_pwm),
        predicted_temp_c=best_temp,
        setpoint_c=setpoint_c,
        cost=best_cost,
        model_status=model_status,
    )
