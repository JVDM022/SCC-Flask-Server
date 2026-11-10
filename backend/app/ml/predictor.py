from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any

import pandas as pd

from .features import FeatureHistoryError, build_feature_state


HORIZONS = (1, 5, 10)
LOGGER = logging.getLogger(__name__)


class ThermalPredictor:
    def __init__(self, model_dir: str | Path):
        self.model_dir = Path(model_dir)
        self.models: dict[int, Any] = {}
        self.feature_columns: list[str] = []
        self.load_errors: dict[int | str, str] = {}
        self._load()

    def _load(self) -> None:
        try:
            import joblib
        except ImportError:
            return

        feature_path = self.model_dir / "feature_columns.json"
        if feature_path.exists():
            try:
                columns = json.loads(feature_path.read_text())
                if not isinstance(columns, list) or not all(isinstance(column, str) for column in columns):
                    raise ValueError("feature_columns.json must contain a JSON list of column names")
                self.feature_columns = columns
            except Exception as exc:
                self.load_errors["features"] = str(exc)
                LOGGER.warning("Could not load model feature columns from %s: %s", feature_path, exc)

        for horizon in HORIZONS:
            model_path = self.model_dir / f"best_model_{horizon}s.joblib"
            if model_path.exists():
                try:
                    self.models[horizon] = joblib.load(model_path)
                except Exception as exc:
                    self.load_errors[horizon] = str(exc)
                    LOGGER.warning("Could not load thermal model %s: %s", model_path, exc)

    def available_horizons(self) -> list[int]:
        return sorted(self.models.keys())

    def _feature_row(
        self,
        telemetry: dict,
        override_pwm: int | None = None,
        history: list[dict] | None = None,
    ) -> pd.DataFrame:
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
        feature_state = build_feature_state(history or [telemetry], override_pwm=override_pwm)
        missing = [column for column in columns if column not in feature_state]
        if missing:
            raise FeatureHistoryError(f"Live feature builder does not provide: {', '.join(missing)}")
        values = [float(feature_state[column]) for column in columns]
        return pd.DataFrame([values], columns=columns, dtype=float)

    def predict(
        self,
        telemetry: dict,
        horizon_s: int,
        override_pwm: int | None = None,
        history: list[dict] | None = None,
    ) -> dict:
        model = self.models.get(horizon_s)
        if model is None:
            return {
                "status": "unavailable",
                "horizon_s": horizon_s,
                "message": f"Model best_model_{horizon_s}s.joblib was not found.",
            }

        current_temp = float(telemetry.get("temp_c", 0.0) or 0.0)
        try:
            features = self._feature_row(telemetry, override_pwm=override_pwm, history=history)
        except FeatureHistoryError as exc:
            return {
                "status": "unavailable",
                "horizon_s": horizon_s,
                "message": str(exc),
            }
        try:
            delta_temp = float(model.predict(features)[0])
        except Exception as exc:
            LOGGER.warning("Thermal model %ss prediction failed: %s", horizon_s, exc)
            return {
                "status": "unavailable",
                "horizon_s": horizon_s,
                "message": f"Model prediction failed: {exc}",
            }
        return {
            "status": "ok",
            "horizon_s": horizon_s,
            "current_temp_c": current_temp,
            "predicted_delta_c": delta_temp,
            "predicted_temp_c": current_temp + delta_temp,
        }

    def predict_all(self, telemetry: dict, history: list[dict] | None = None) -> dict:
        return {
            "status": "ok" if self.models else "unavailable",
            "available_horizons": self.available_horizons(),
            "predictions": [self.predict(telemetry, horizon, history=history) for horizon in HORIZONS],
        }
