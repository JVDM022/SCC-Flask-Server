from __future__ import annotations

from dataclasses import replace
from typing import Any

from flask import current_app
from sqlalchemy import and_, or_

from ..database.db import db
from ..database.models import ControlCommand, MpcRecommendation, Telemetry
from ..ml.predictor import ThermalPredictor
from ..mpc.advisory import MpcResult, recommend_heater_pwm


MPC_COMMAND_TYPE = "MPC_CONTROL"
LEGACY_MPC_COMMAND_TYPE = "MPC_PWM"
MPC_HISTORY_ROWS = 10


def closed_loop_enabled() -> bool:
    return bool(current_app.config.get("MPC_CLOSED_LOOP_ENABLED", False))


def get_predictor() -> ThermalPredictor:
    model_dir = str(current_app.config["MODEL_DIR"])
    cache_key = "scc_thermal_predictor"
    cached = current_app.extensions.get(cache_key)
    if cached is None or str(cached.model_dir) != model_dir:
        cached = ThermalPredictor(model_dir)
        current_app.extensions[cache_key] = cached
    return cached


def recent_telemetry_history(latest: Telemetry, limit: int = MPC_HISTORY_ROWS) -> list[dict[str, Any]]:
    query = Telemetry.query.filter(Telemetry.id <= latest.id)
    if latest.device_id:
        query = query.filter(Telemetry.device_id == latest.device_id)
    rows = query.order_by(Telemetry.id.desc()).limit(limit).all()
    return [row.to_dict() for row in reversed(rows)]


def _history_is_contiguous(history: list[dict[str, Any]]) -> bool:
    if len(history) < MPC_HISTORY_ROWS:
        return False
    max_gap_ms = int(float(current_app.config.get("MPC_MAX_SAMPLE_GAP_SECONDS", 3.0)) * 1000)
    milliseconds = [row.get("ms") for row in history]
    if any(value is None for value in milliseconds):
        return False
    return all(0 <= int(current) - int(previous) <= max_gap_ms for previous, current in zip(milliseconds, milliseconds[1:]))


def _add_preheat_plan(
    result: MpcResult,
    row: dict[str, Any],
    predictor: ThermalPredictor,
    history: list[dict[str, Any]],
) -> MpcResult:
    """Estimate the next pump disturbance and produce a bounded preheat target."""

    setpoint_c = float(row.get("setpoint_c") or 0.0)
    current_temp_c = float(row.get("temp_c") or setpoint_c)
    max_pwm = int(current_app.config.get("MPC_MAX_PWM", 200))
    probe = dict(row)
    probe.update(
        {
            "event": 1,
            "pump_on": 1,
            "motor_pwm": int(current_app.config.get("MPC_PUMP_PROBE_PWM", 155)),
            "heater_pwm": max_pwm,
        }
    )
    probe_history = [dict(item) for item in history]
    probe_history[-1] = probe
    prediction = predictor.predict(
        probe,
        5,
        override_pwm=max_pwm,
        history=probe_history,
    )
    if prediction.get("status") != "ok":
        return replace(result, preheat_target_c=setpoint_c)

    predicted_quench_c = float(prediction["predicted_temp_c"])
    predicted_drop_c = max(0.0, current_temp_c - predicted_quench_c)
    observed_drop_c = max(0.0, float(row.get("last_pump_drop_c") or 0.0))
    compensated_drop_c = max(predicted_drop_c, observed_drop_c)
    preheat_limit_c = max(setpoint_c, float(current_app.config.get("MPC_PREHEAT_MAX_C", 128.0)))
    preheat_target_c = min(preheat_limit_c, setpoint_c + compensated_drop_c)
    return replace(
        result,
        predicted_pump_drop_c=compensated_drop_c,
        preheat_target_c=preheat_target_c,
    )


def _recommend_with_preheat(
    row: dict[str, Any],
    predictor: ThermalPredictor,
    history: list[dict[str, Any]],
    *,
    advisory_only: bool,
) -> MpcResult:
    result = recommend_heater_pwm(
        row,
        predictor=predictor,
        history=history,
        advisory_only=advisory_only,
    )
    if result.status != "ok" or result.model_status != "ok":
        return result

    plan = _add_preheat_plan(result, row, predictor, history)
    if plan.preheat_target_c is None:
        return plan

    optimized = recommend_heater_pwm(
        row,
        predictor=predictor,
        history=history,
        advisory_only=advisory_only,
        objective_setpoint_c=plan.preheat_target_c,
    )
    return replace(
        optimized,
        predicted_pump_drop_c=plan.predicted_pump_drop_c,
        preheat_target_c=plan.preheat_target_c,
    )


def recommendation_for(latest: Telemetry, *, advisory_only: bool | None = None) -> MpcResult:
    history = recent_telemetry_history(latest)
    predictor = get_predictor()
    if len(history) < MPC_HISTORY_ROWS or 5 not in predictor.available_horizons():
        return recommend_heater_pwm(
            latest.to_dict(),
            predictor=predictor,
            history=history,
            advisory_only=not closed_loop_enabled() if advisory_only is None else advisory_only,
        )
    return _recommend_with_preheat(
        latest.to_dict(),
        predictor,
        history,
        advisory_only=not closed_loop_enabled() if advisory_only is None else advisory_only,
    )


def process_closed_loop(latest: Telemetry) -> MpcResult | None:
    """Queue a guarded MPC PWM command for the NUC gateway.

    The loop only runs in firmware PID-hold mode (telemetry mode 4), with a
    loaded 5-second model and a contiguous telemetry window. If updates stop,
    the Arduino watchdog falls back to local PID control.
    """

    if not closed_loop_enabled():
        return None

    row = latest.to_dict()
    if int(row.get("mode") or 0) != 4:
        return None
    if any(int(row.get(key) or 0) == 1 for key in ("manual_kill", "hard_kill", "heater_lockout")):
        return None

    pending_stop = (
        ControlCommand.query.filter_by(applied=False)
        .filter(
            or_(
                ControlCommand.command_type == "KILL",
                and_(ControlCommand.command_type == "SET_ON", ControlCommand.value == 0),
                and_(ControlCommand.command_type == "SET_HEATER", ControlCommand.value == 0),
            )
        )
        .first()
    )
    if pending_stop is not None:
        return None

    history = recent_telemetry_history(latest)
    if not _history_is_contiguous(history):
        return None

    predictor = get_predictor()
    if 5 not in predictor.available_horizons():
        return None

    result = _recommend_with_preheat(row, predictor, history, advisory_only=False)
    if result.status != "ok" or result.model_status != "ok":
        return result

    db.session.add(
        MpcRecommendation(
            current_temp_c=latest.temp_c,
            setpoint_c=latest.setpoint_c,
            current_pwm=latest.heater_pwm,
            recommended_pwm=result.recommended_pwm,
            predicted_temp_c=result.predicted_temp_c,
            cost=result.cost,
        )
    )
    ControlCommand.query.filter(
        ControlCommand.command_type.in_([MPC_COMMAND_TYPE, LEGACY_MPC_COMMAND_TYPE]),
        ControlCommand.applied.is_(False),
    ).delete(synchronize_session=False)
    db.session.add(
        ControlCommand(
            command_type=MPC_COMMAND_TYPE,
            value=max(0, min(int(current_app.config.get("MPC_MAX_PWM", 200)), result.recommended_pwm)),
            setpoint_c=float(result.preheat_target_c or latest.setpoint_c or 0.0),
        )
    )
    return result


def runtime_status() -> dict[str, Any]:
    predictor = get_predictor()
    latest_command = (
        ControlCommand.query.filter(ControlCommand.command_type.in_([MPC_COMMAND_TYPE, LEGACY_MPC_COMMAND_TYPE]))
        .order_by(ControlCommand.id.desc())
        .first()
    )
    latest_recommendation = MpcRecommendation.query.order_by(MpcRecommendation.id.desc()).first()
    return {
        "status": "enabled" if closed_loop_enabled() else "disabled",
        "closed_loop_enabled": closed_loop_enabled(),
        "available_horizons": predictor.available_horizons(),
        "model_errors": predictor.load_errors,
        "required_horizon_s": 5,
        "max_pwm": int(current_app.config.get("MPC_MAX_PWM", 200)),
        "preheat_max_c": float(current_app.config.get("MPC_PREHEAT_MAX_C", 128.0)),
        "firmware_watchdog_ms": int(current_app.config.get("MPC_FIRMWARE_WATCHDOG_MS", 3500)),
        "latest_command": latest_command.to_dict() if latest_command else None,
        "latest_recommendation": latest_recommendation.to_dict() if latest_recommendation else None,
    }
