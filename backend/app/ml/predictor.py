from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import numpy as np


HORIZONS = (1, 5, 10)


class ThermalPredictor:
    def __init__(self, model_dir: str | Path):
        self.model_dir = Path(model_dir)
        self.models: dict[int, Any] = {}
        self.feature_columns: list[str] = []
        self._load()

    def _load(self) -> None:
        try:
            import joblib
        except ImportError:
            return

        feature_path = self.model_dir / "feature_columns.json"
        if feature_path.exists():
            self.feature_columns = json.loads(feature_path.read_text())

        for horizon in HORIZONS:
            model_path = self.model_dir / f"best_model_{horizon}s.joblib"
            if model_path.exists():
                self.models[horizon] = joblib.load(model_path)

    def available_horizons(self) -> list[int]:
        return sorted(self.models.keys())

    def _feature_row(self, telemetry: dict, override_pwm: int | None = None) -> np.ndarray:
        columns = self.feature_columns or [
            "temp_c",
            "dtemp_c_per_s",
            "setpoint_c",
            "heater_pwm",
            "pump_on",
            "motor_pwm",
            "last_pump_drop_c",
            "recovery_time_s",
        ]
        values = []
        for column in columns:
            if column == "heater_pwm" and override_pwm is not None:
                values.append(float(override_pwm))
            else:
                values.append(float(telemetry.get(column, 0.0) or 0.0))
        return np.array([values], dtype=float)

    def predict(self, telemetry: dict, horizon_s: int, override_pwm: int | None = None) -> dict:
        model = self.models.get(horizon_s)
        if model is None:
            return {
                "status": "unavailable",
                "horizon_s": horizon_s,
                "message": f"Model best_model_{horizon_s}s.joblib was not found.",
            }

        current_temp = float(telemetry.get("temp_c", 0.0) or 0.0)
        features = self._feature_row(telemetry, override_pwm=override_pwm)
        delta_temp = float(model.predict(features)[0])
        return {
            "status": "ok",
            "horizon_s": horizon_s,
            "current_temp_c": current_temp,
            "predicted_delta_c": delta_temp,
            "predicted_temp_c": current_temp + delta_temp,
        }

    def predict_all(self, telemetry: dict) -> dict:
        return {
            "status": "ok" if self.models else "unavailable",
            "available_horizons": self.available_horizons(),
            "predictions": [self.predict(telemetry, horizon) for horizon in HORIZONS],
        }
