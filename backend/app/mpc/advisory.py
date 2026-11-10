from __future__ import annotations

from dataclasses import dataclass


candidate_pwm_values = [0, 32, 64, 96, 128, 160, 184, 200]
FULL_HEAT_ERROR_C = 2.0
MINIMUM_HEAT_ERROR_C = 0.5


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
    predicted_pump_drop_c: float | None = None
    preheat_target_c: float | None = None

    def to_dict(self) -> dict:
        return self.__dict__.copy()


def _clamp_pwm(value: int) -> int:
    return max(0, min(200, int(value)))


def _fallback_predict_temp(row: dict, candidate_pwm: int) -> float:
    current_temp = float(row.get("temp_c", 0.0) or 0.0)
    dtemp = float(row.get("dtemp_c_per_s", 0.0) or 0.0)
    pwm_delta = candidate_pwm - int(row.get("heater_pwm", 0) or 0)
    return current_temp + (5.0 * dtemp) + (0.002 * pwm_delta)


def _guarded_candidates(current_temp_c: float, setpoint_c: float, current_pwm: int) -> list[int]:
    """Prevent reverse-causality artifacts from commanding too little heat.

    In observational training data, zero PWM often appears immediately after a
    hot period, so a regressor can incorrectly associate zero PWM with rising
    temperature. These minimum-output constraints retain model selection near
    setpoint while keeping recovery direction physically valid.
    """

    candidates = set(candidate_pwm_values + [current_pwm])
    error_c = setpoint_c - current_temp_c
    if error_c >= FULL_HEAT_ERROR_C:
        candidates = {candidate for candidate in candidates if candidate >= 184}
    elif error_c >= MINIMUM_HEAT_ERROR_C:
        candidates = {candidate for candidate in candidates if candidate >= 128}
    return sorted(candidates)


def recommend_heater_pwm(
    row: dict,
    predictor=None,
    *,
    history: list[dict] | None = None,
    advisory_only: bool = True,
    objective_setpoint_c: float | None = None,
) -> MpcResult:
    current_pwm = _clamp_pwm(int(row.get("heater_pwm", 0) or 0))
    current_temp_c = float(row.get("temp_c", 0.0) or 0.0)
    setpoint_c = float(row.get("setpoint_c", 0.0) or 0.0)
    objective_c = setpoint_c if objective_setpoint_c is None else float(objective_setpoint_c)

    if row.get("hard_kill") == 1 or row.get("manual_kill") == 1 or row.get("heater_lockout") == 1:
        return MpcResult(
            status="safety_override",
            current_pwm=current_pwm,
            recommended_pwm=0,
            predicted_temp_c=None,
            setpoint_c=setpoint_c,
            cost=0.0,
            model_status="bypassed_by_safety",
            advisory_only=advisory_only,
        )

    best_pwm = 0
    best_temp: float | None = None
    best_cost: float | None = None
    model_status = "fallback"

    candidates = _guarded_candidates(current_temp_c, objective_c, current_pwm)
    for candidate in candidates:
        candidate = _clamp_pwm(candidate)
        if predictor is not None and 5 in predictor.available_horizons():
            prediction = predictor.predict(row, 5, override_pwm=candidate, history=history)
            if prediction.get("status") == "ok":
                predicted_temp = float(prediction["predicted_temp_c"])
                model_status = "ok"
            else:
                predicted_temp = _fallback_predict_temp(row, candidate)
                model_status = "fallback"
        else:
            predicted_temp = _fallback_predict_temp(row, candidate)

        normalized_effort = candidate / 200.0
        normalized_move = (candidate - current_pwm) / 200.0
        cost = ((predicted_temp - objective_c) ** 2) + (0.01 * normalized_effort**2) + (
            0.05 * normalized_move**2
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
        advisory_only=advisory_only,
    )
