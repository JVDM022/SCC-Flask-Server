# Arduino Firmware

Imported firmware project:

- `SCC-V1.4`: Arduino SCC controller firmware source and tests.

This firmware owns the thermal SCC rig control behavior. The ESP32 project under `firmware/esp32/ESP32-Relay` is only the relay/communications bridge.

The backend expects telemetry in the fixed CSV order documented in `docs/telemetry_format.md`.
