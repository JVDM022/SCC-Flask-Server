# MPC Design

The MPC module evaluates a fixed set of candidate heater PWM values:

```python
[0, 64, 128, 184, 220, 255]
```

For each candidate, it estimates the 5-second future temperature using `models/best_model_5s.joblib` when present. Live inference recreates the same lag and rolling features used by the training notebook from the ten most recent device samples. If no model is available, the endpoint can display a fallback estimate, but fallback results are never sent to the Arduino.

Closed-loop output is opt-in through `MPC_CLOSED_LOOP_ENABLED=1`. Commands are only queued when the firmware reports PID-hold mode, the 5-second model is loaded, telemetry history is contiguous, and no kill, lockout, or pending stop command exists. The Arduino accepts the override only while the heater is already enabled, clamps it to PWM 200, preserves pump pre-bias, applies a target-plus-0.5 C ceiling, and returns to local PID if no new command arrives within 3.5 seconds.

Because observational data can encode reverse causality, supervisory recovery floors prevent the regressor from choosing low PWM far below setpoint. This is a hybrid safety constraint, not a claim that the learned model is causal.

The same 5-second model is evaluated with a counterfactual pump-start input to estimate the next pump disturbance. The controller adds the larger of that predicted drop and the last measured drop to the operator setpoint, then caps the temporary target at `MPC_PREHEAT_MAX_C`. Firmware will not start the next pump dose until the bounded preheat target is reached. Backend configuration cannot bypass the firmware's independent preheat limit, heater lockout, or hard-kill threshold.

The cost function is:

```text
cost = (predicted_temp - setpoint_c)^2 + 0.0005*pwm^2 + 0.05*(pwm - previous_pwm)^2
```

If hard kill, manual kill, or heater lockout is active, the recommendation is forced to PWM 0.
