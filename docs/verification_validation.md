# 9. Verification and Validation

## 9.1. Verification and Validation Overview

The SCC platform has implemented automated unit tests for firmware logic, NUC gateway command construction, ESP32 legacy relay parsing, backend parsing, alarms, firmware APIs, authentication, and MPC safety overrides. The repository also includes manual integration procedures for telemetry pipeline validation and standalone hardware test sketches for the thermistor, heater, and motor.

The tables below list only tests and validation procedures that are present in the repository. Items that are only planned, such as dashboard automation, heartbeat monitoring, and full end-to-end system commissioning tests, are not included.

## 9.2. Unit Tests

| Test ID | Description | Expected Result | Status |
| --- | --- | --- | --- |
| UT-001 | Temperature conversion and ADC calibration | ADC values are converted to expected temperature values using the calibrated transfer function | Implemented |
| UT-002 | ADC sensor fault detection | Open-circuit and short-circuit ADC rail values are detected with recovery hysteresis | Implemented |
| UT-003 | Pump temperature gating | Pump permission enables only with thermal headroom and disables at low or high temperature limits | Implemented |
| UT-004 | Pump PWM timing | Pump startup kick, run PWM, and off timing follow the configured motor cycle | Implemented |
| UT-005 | Motor timer and heater pre-bias logic | Motor cycle phase and heater bias windows are calculated correctly | Implemented |
| UT-006 | Autotune and PID state logic | Autotune transitions, timeout behavior, PID first-sample behavior, ramp completion, and lockout output suppression work as expected | Implemented |
| UT-007 | Telemetry CSV schema | Telemetry header column count and column order match the expected schema | Implemented |
| UT-008 | Bootloader entry command parsing | Supported bootloader preparation commands are accepted and unrelated commands are rejected | Implemented |
| UT-009 | ESP32 relay CSV parsing | Arduino CSV telemetry rows are parsed into the relay snapshot fields correctly | Implemented |
| UT-010 | ESP32 relay backend command parsing | Supported backend command types are parsed and unsupported commands are rejected | Implemented |
| UT-011 | Backend telemetry parser | Headerless and header-based CSV telemetry rows are parsed and invalid rows are rejected | Implemented |
| UT-012 | Backend alarm rules | Hard kill, manual kill, heater lockout, sensor fault, high temperature, slow recovery, and excessive cooling alarms are generated correctly | Implemented |
| UT-013 | Backend API authentication | Protected telemetry write endpoints reject missing keys and accept valid API key formats | Implemented |
| UT-014 | Firmware API validation and command queue | ESP32 and Arduino firmware file types are validated and queued commands can be acknowledged | Implemented |
| UT-015 | MPC safety override | MPC recommendations return zero PWM during hard kill, manual kill, or heater lockout states | Implemented |
| UT-016 | Intel NUC gateway command mapping | Backend commands map to Arduino USB serial lines and Arduino `.hex` flashing uses the expected `avrdude` command | Implemented |

## 9.3. Integration and Hardware Validation Procedures

| Test ID | Description | Expected Result | Status |
| --- | --- | --- | --- |
| IT-001 | Arduino USB telemetry to NUC gateway parsing | The NUC gateway receives deterministic Arduino CSV rows and parses them with the backend telemetry parser | Manual procedure defined |
| IT-002 | NUC gateway telemetry relay to Flask API | The NUC gateway posts parsed telemetry JSON to the Flask telemetry endpoint | Manual procedure defined |
| IT-003 | Flask telemetry ingestion to PostgreSQL | Stored PostgreSQL rows match the source Arduino CSV fixture | Manual verifier implemented |
| IT-004 | End-to-end telemetry pipeline | Data propagate from Arduino test firmware through the NUC gateway and Flask into PostgreSQL | Manual procedure defined |
| IT-005 | Backend-only telemetry pipeline smoke test | Sample CSV rows are posted directly to Flask and verified against PostgreSQL without hardware | Scripted verifier implemented |
| IT-006 | Standalone NTC temperature readout | Arduino reads thermistor ADC values and prints live temperature CSV rows | Manual hardware test project defined |
| IT-007 | Standalone heater control test | Arduino heater test accepts start/stop commands, limits PWM, and reports hard-kill status | Manual hardware test project defined |
| IT-008 | Standalone motor output test | Arduino motor test accepts start/stop commands and reports PWM output state | Manual hardware test project defined |

## 9.4. Current Coverage Gaps

The repository does not currently include automated tests for dashboard live updates, physical USB serial I/O, device heartbeat/offline detection, full HMI-to-Arduino manual kill routing on hardware, or complete long-duration system commissioning. These should remain listed as future validation work unless separate test evidence is produced.
