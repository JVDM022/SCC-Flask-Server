# ESP32 Relay Firmware

This firmware bridges UART telemetry from an Arduino-class controller to HTTPS or Azure IoT Hub MQTT, forwards backend commands to the Arduino, and supports OTA updates for the ESP32 firmware itself.

## ESP32 OTA

ESP32 OTA is supported through the existing `OTA` backend command and optional `OTA_FIRMWARE_URL` configuration. A command may provide a firmware URL:

```json
{"cmdId": 10, "type": "OTA", "url": "https://example.test/esp32-relay.bin"}
```

If `url` is omitted, the firmware falls back to `OTA_FIRMWARE_URL`.

## Arduino OTA Scaffolding

The firmware recognizes an `ARDUINO_OTA` backend command as scaffolding for future Arduino-over-UART flashing:

```json
{"cmdId": 11, "type": "ARDUINO_OTA", "url": "https://example.test/arduino-uno.hex"}
```

The same command name is accepted as an Azure IoT Hub direct method with a JSON payload containing `url`.

Current behavior:

- Normal Arduino command forwarding is paused while Arduino OTA preparation is active.
- The firmware validates the URL, records the configured max firmware size, and prepares the download path.
- If `ARDUINO_RESET_GPIO` is configured, the ESP32 safely pulses Arduino RESET low, then high.
- If reset wiring is missing, the firmware logs that bootloader entry is unsupported without wiring or Arduino firmware cooperation.
- The firmware does not flash the Arduino yet.

Actual Arduino flashing still requires a tested STK500v1/Optiboot programmer implementation over UART. Do not treat `ARDUINO_OTA` as a complete firmware updater until that programmer exists.

## Arduino Reset Wiring

Arduino Uno bootloader flashing is most reliable when the ESP32 can control Arduino RESET. Configure:

```c
#define ARDUINO_OTA_ENABLED 1
#define ARDUINO_RESET_GPIO 25
```

Use `-1` when RESET is not wired:

```c
#define ARDUINO_RESET_GPIO -1
```

Software-only bootloader entry is possible only if the Arduino firmware implements a command that jumps to the bootloader or resets itself. That approach is less reliable than a hardware RESET line and may fail if the running Arduino firmware is corrupted or wedged.

## Configuration

Copy `include/wifi_credentials_local.example.h` to `include/wifi_credentials_local.h` and set local values. Arduino OTA-related options:

```c
#define ARDUINO_OTA_ENABLED 0
#define ARDUINO_RESET_GPIO -1
#define ARDUINO_OTA_MAX_FIRMWARE_BYTES 32768U
#define ARDUINO_OTA_MAX_URL_LEN 320
```

Keep `ARDUINO_OTA_ENABLED` disabled until reset wiring and the STK500v1/Optiboot programmer path are implemented and tested.
