# MPC Design

The initial MPC module is advisory only. It evaluates a fixed set of candidate heater PWM values:

```python
[0, 64, 128, 184, 220, 255]
```

For each candidate, it estimates the 5-second future temperature using `models/best_model_5s.joblib` when present. If no model is available, the backend uses a simple fallback predictor so the endpoint remains usable during commissioning.

The cost function is:

```text
cost = (predicted_temp - setpoint_c)^2 + 0.0005*pwm^2 + 0.05*(pwm - previous_pwm)^2
```

If hard kill, manual kill, or heater lockout is active, the recommendation is forced to PWM 0.
