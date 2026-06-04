# Model Artifacts

Optional model files:

- `best_model_1s.joblib`
- `best_model_5s.joblib`
- `best_model_10s.joblib`
- `feature_columns.json`

Each model should predict delta temperature for its horizon. The backend reconstructs future temperature as `current_temp_c + predicted_delta_c`.
