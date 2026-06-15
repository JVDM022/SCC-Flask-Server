# 11. Operation Guide

## 11.1. Startup Checks

Before starting a test run, verify that the heater, pump, thermistor, Arduino controller, ESP32 relay, backend, database, MQTT broker, and HMI dashboard are powered and communicating. Confirm that the latest telemetry updates in the dashboard, no critical alarm is active, and the manual kill state is clear.

## 11.2. Normal Operation

**Operator responsibilities:**

- Confirm that the SCC rig is physically safe to operate before enabling heat or pump activity.
- Keep the Operations dashboard open during testing and verify that telemetry is updating approximately every few seconds.
- Monitor current temperature, setpoint, temperature error, heater PWM, pump status, and controller mode.
- Verify that the bath remains within the intended operating window and that heater lockout, manual kill, and hard kill indicators remain clear.
- Confirm that pump cycles occur only when the controller reports sufficient thermal headroom.
- Review active alarms and event timeline entries during each operating session.
- Do not treat ML predictions or MPC recommendations as safety controls; they are advisory only.
- Record abnormal behavior, operator actions, alarms, and shutdown events in the test log.
- Stop the test using the manual emergency control or physical disconnect if unsafe conditions are observed.

**Alarm response procedures:**

| Alarm | Severity | Operator response |
| --- | --- | --- |
| `HARD_KILL` | Critical | Treat as an emergency shutdown. Verify heater PWM is `0`, pump/motor output is off, allow the bath to cool, inspect hardware, and do not restart until the cause is identified. |
| `MANUAL_KILL` | Critical | Confirm the manual shutdown was intentional. Keep outputs disabled until the rig is inspected and the operator is ready to clear the manual kill state. |
| `SENSOR_FAULT` | Critical | Stop operation, inspect thermistor wiring and ADC input, verify readings with an independent thermometer if available, and restart only after valid telemetry is restored. |
| `HIGH_TEMP_CRITICAL` | Critical | Stop heating immediately, verify heater output is de-energized, allow cooling, and inspect for heater control or sensor failure. |
| `HEATER_LOCKOUT` | Warning | Continue monitoring while the heater remains disabled. Do not clear or restart until temperature falls below the lockout reset threshold and no other fault is active. |
| `HIGH_TEMP_WARNING` | Warning | Watch temperature trend and heater PWM closely. Prepare to stop the test if temperature continues rising. |
| `SLOW_RECOVERY` | Warning | Inspect pump operation, flow path, bath mixing, and temperature recovery behavior after pump cycles. |
| `EXCESSIVE_PUMP_COOLING` | Warning | Check pump dose duration, flow rate, solution temperature, and thermal compensation settings. |

If any critical alarm occurs, the operator must prioritize physical safety over data collection. The test should remain stopped until the root cause is reviewed.

**Monitoring requirements:**

- Temperature, setpoint, and temperature error shall be monitored continuously during active heating.
- Heater PWM shall be checked to confirm that heating output responds normally and drops to `0` during lockout, manual kill, hard kill, or sensor fault states.
- Pump status, motor PWM, pump recovery time, and last pump temperature drop shall be monitored to identify blocked flow, excessive cooling, or failed recovery.
- Alarm status shall be reviewed through the active alarm panel during operation.
- Event timeline entries for pump start, pump end, pump recovery, hard kill, and sensor fault events shall be reviewed after each test run.
- MQTT/backend connectivity shall be checked if dashboard values stop updating.
- PostgreSQL data logging shall be verified by confirming that recent telemetry appears in the history and latest telemetry endpoints.
- Operators shall remain near the rig during active heating unless an independent physical safety cutoff and approved unattended-operation procedure are in place.
- Any abnormal noise, smell, overheating, fluid leak, unexpected pump behavior, or stale telemetry shall trigger test pause or shutdown.
