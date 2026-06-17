# 8. Safety Analysis

The SCC control platform is designed so safety-critical decisions remain on the hardware side of the system. The Arduino controller and electrical interlocks own heater and pump shutdown behavior. The Intel NUC gateway, ESP32 legacy relay, Flask backend, database, HMI, ML model, and MPC service provide communication, monitoring, logging, alarms, and advisory recommendations only.

Backend alarms and dashboard notifications improve operator visibility, but they are not the primary protection layer. The rig must remain safe if the network, MQTT broker, database, server, HMI, ML model, or MPC endpoint fails.

## 8.1. Hazards

| Hazard | Consequence | Existing mitigation |
| --- | --- | --- |
| Bath overheating | Equipment damage, accelerated corrosion, unsafe test conditions, or operator burn risk | Firmware heater lockout at `128.50 C`, hard kill at `130.00 C`, heater PWM capped at `200`, and backend high-temperature alarms |
| Sensor open circuit, short circuit, or invalid ADC reading | Controller may heat using false temperature feedback | ADC rail checks trigger `SENSOR_FAULT`, force heater off, stop pump output, set heater lockout, and emit alarm telemetry |
| Pump failure or blocked flow | Excessive cooling disturbance, poor solution mixing, invalid test data, or delayed thermal recovery | Pump cycle telemetry records start/end/recovery, backend alarms flag slow recovery over `60 s`, and pump dosing is temperature-gated |
| Pump running without thermal headroom | Bath temperature can fall below the SCC operating window | Pump is only allowed at or above `127.00 C`, disabled at or below `123.00 C`, and disabled at/above `130.00 C` |
| Heater stuck on or excessive PWM command | Thermal runaway and damage to the bath, heater, specimen, or enclosure | Firmware lockout/hard kill drives heater command to zero; final design should include an independent power disconnect or hard kill relay |
| Manual emergency stop needed | Operator may be unable to stop the rig quickly through normal controls | Manual `KILL 1` command forces heater off, stops motor output, clears pump command, resets PID integral, and sets lockout |
| Network, MQTT, server, database, or HMI loss | Loss of monitoring, alarms, or remote command capability | Local firmware continues control and safety logic without backend availability; telemetry loss is treated as an operational fault |
| ML or MPC recommendation error | Unsafe actuator command if advisory output is treated as authoritative | MPC remains advisory; when heater lockout, manual kill, or hard kill is active, recommendation returns zero PWM |
| Firmware update interruption | Controller may enter an unknown state during update preparation | OTA preparation explicitly turns heater off, stops motor output, disables normal control output, and requires tested reset/programming wiring |
| Excessive runtime or unattended operation | Long-duration heating can continue after intended test window | Firmware runtime limit disables heater and motor after `96 hours` while telemetry continues |

## 8.2. Safety Features

- **Heater lockout:** Heater output is forced off when the measured temperature reaches `128.50 C`. Lockout clears only after the bath cools to `126.50 C`.
- **Hard kill threshold:** At `130.00 C`, firmware forces heater PWM to `0`, stops the motor output, clears pump command, sets heater lockout, clears the PID integral term, and emits a hard-kill event.
- **Manual kill command:** The backend/HMI can queue a `KILL 1` command through the Intel NUC gateway. When received by the Arduino, this immediately disables heater and motor outputs and prevents new pump activity.
- **Sensor fault detection:** ADC values near the rails are treated as thermistor faults. During a fault, the controller disables heater and motor outputs, sets lockout, and reports a `SENSOR_FAULT` event.
- **Safe startup sequence:** On boot, heater and motor outputs are set off before control state is initialized. Autotune begins from a known state, with telemetry header output enabled for monitoring.
- **Pump temperature gating:** Pump dosing only occurs when the bath has enough thermal headroom. The pump is disabled below the low threshold and is not allowed near the hard-kill limit.
- **PWM limiting:** Heater PWM is clamped to the configured maximum so software cannot command full-range output accidentally.
- **PID windup protection:** The integral term is reset during lockout, manual kill, hard kill, and sensor fault states, reducing overshoot when the system returns to service.
- **Watchdog/reset-safe OTA preparation:** Optional bootloader entry preparation drives heater and motor outputs off before reset handoff.
- **Backend alarms and audit trail:** The backend records critical states including hard kill, manual kill, sensor fault, high temperature, slow recovery, and excessive pump cooling.
- **Advisory-only MPC:** Model-based control recommendations do not override safety states. Safety states return `recommended_pwm = 0`.

## 8.3. Emergency Shutdown Procedure

1. Activate the physical hard kill or power disconnect if installed.
2. Send the manual kill command from the HMI or backend (`KILL 1`).
3. Verify heater PWM is `0` and the heater output is electrically de-energized.
4. Verify pump/motor output is off.
5. Allow the bath to cool below the heater lockout reset threshold before clearing the fault.
6. Record the event, timestamp, measured temperature, alarm state, and operator action.
7. Inspect the heater, thermistor wiring, pump path, power electronics, and enclosure before restarting.
8. Restart only after the root cause is identified and the operator confirms the rig is safe.

## 8.4. Verification Tests

| Safety function | Test method | Expected result |
| --- | --- | --- |
| Safe startup | Boot controller while observing heater and motor outputs | Outputs remain off during initialization and only enable through control logic |
| Heater lockout | Simulate or heat to `128.50 C` | Heater PWM becomes `0`; `heater_lockout = 1` in telemetry |
| Lockout recovery | Cool below `126.50 C` | Lockout clears and normal control can resume if no other fault is active |
| Hard kill | Simulate temperature at or above `130.00 C` | Heater and motor outputs are off; `hard_kill = 1`; event code `4` is emitted |
| Sensor fault | Open or short thermistor input, or simulate ADC rail reading | Heater and motor outputs are off; event code `5` is emitted; backend raises `SENSOR_FAULT` |
| Manual kill | Send `KILL 1` through command path | Heater and motor outputs are off; `manual_kill = 1`; pump command clears |
| Pump gating | Test pump behavior below `123.00 C`, at/above `127.00 C`, and near `130.00 C` | Pump only runs inside the allowed temperature window |
| Runtime limit | Simulate elapsed runtime at `96 hours` | Heater and motor are disabled while telemetry continues |
| Backend alarm path | Submit telemetry rows for hard kill, manual kill, sensor fault, high temperature, slow recovery, and excessive pump cooling | Alarms are created with correct severity and message |
| MPC safety override | Request MPC recommendation during lockout, manual kill, or hard kill | Recommendation returns zero PWM |

## 8.5. Residual Risks and Required Controls

- A physical emergency stop or power disconnect should be verified independently of firmware before unattended operation.
- The thermistor calibration and ADC fault thresholds must be validated against real open-circuit, short-circuit, and high-temperature conditions.
- Heater MOSFET or relay failure can defeat software PWM commands; final hardware should include independent over-temperature cutoff or fused power isolation.
- Network loss removes remote visibility, so local indicators or periodic operator checks are still required during tests.
- OTA update paths must remain disabled or limited until reset wiring and programming behavior have been tested on the actual hardware.
- Any future closed-loop use of MPC recommendations requires a separate safety review, command authentication review, and proof that firmware safety overrides cannot be bypassed.
