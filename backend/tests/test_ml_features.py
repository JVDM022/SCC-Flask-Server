from __future__ import annotations

import math

import pytest

from app.ml.features import FeatureHistoryError, build_feature_state
from app.ml.predictor import ThermalPredictor


def history_rows() -> list[dict]:
    rows = []
    for index in range(10):
        rows.append(
            {
                "temp_c": 100.0 + index,
                "heater_pwm": index * 10,
                "pump_on": 1 if index in {3, 9} else 0,
                "event": 1 if index == 9 else 0,
                "motor_pwm": 155 if index == 9 else 0,
                "dtemp_c_per_s": index / 10.0,
                "setpoint_c": 125.0,
                "heater_lockout": 0,
            }
        )
    return rows


def test_build_feature_state_matches_training_lags_and_rolling_features():
    features = build_feature_state(history_rows(), override_pwm=200)

    assert features["temp_c_lag_1"] == 108.0
    assert features["temp_c_lag_2"] == 107.0
    assert features["temp_c_lag_5"] == 104.0
    assert features["heater_pwm_lag_1"] == 80.0
    assert features["heater_pwm_lag_2"] == 70.0
    assert features["temp_mean_5"] == 107.0
    assert features["temp_std_5"] == pytest.approx(math.sqrt(2.5))
    assert features["pwm_mean_5"] == 92.0
    assert features["pump_recent_10"] == 1.0
    assert features["temp_error"] == 16.0
    assert features["pwm_delta"] == 120.0
    assert features["pump_event_active"] == 1.0
    assert features["mode_pump_start"] == 1.0
    assert features["mode_pump_running"] == 1.0
    assert features["mode_normal"] == 0.0


def test_build_feature_state_rejects_missing_lag_history():
    with pytest.raises(FeatureHistoryError, match="At least 6"):
        build_feature_state(history_rows()[:5])


def test_predictor_ignores_corrupt_model_artifact(tmp_path):
    (tmp_path / "best_model_5s.joblib").write_bytes(b"not-a-joblib-model")

    predictor = ThermalPredictor(tmp_path)

    assert predictor.available_horizons() == []
    assert 5 in predictor.load_errors
