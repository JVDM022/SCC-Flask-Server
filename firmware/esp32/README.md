# ESP32 Firmware

Imported firmware project:

- `ESP32-Relay`: ESP32 PlatformIO firmware used only as the relay/communications bridge.

Generated `.pio` build outputs and macOS ZIP metadata are intentionally excluded. Create `ESP32-Relay/include/wifi_credentials_local.h` locally from `ESP32-Relay/include/wifi_credentials_local.example.h`; the real credentials header is intentionally not stored in this repo.

The Arduino controller owns the SCC control logic. This ESP32 firmware should be treated as the relay path between the hardware-side controller and backend services.
