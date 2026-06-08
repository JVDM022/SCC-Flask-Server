# ESP32 Relay Firmware

This firmware bridges UART telemetry from an Arduino-class controller to a local MQTT broker, forwards backend commands to the Arduino over HTTP polling, and supports OTA updates for the ESP32 firmware itself.

## Local MQTT Telemetry

Telemetry is published to Mosquitto using the same topic schema consumed by the Flask backend:

```text
scc/site/<site_id>/rig/<rig_id>/device/<device_id>/telemetry
```

Configure the broker and device identity in `include/wifi_credentials_local.h`:

```c
#define MQTT_BROKER_URI "mqtt://YOUR_COMPUTER_LAN_IP:1883"
#define MQTT_BASE_TOPIC "scc"
#define MQTT_SITE_ID "site-01"
#define MQTT_RIG_ID "rig-01"
#define MQTT_DEVICE_ID "esp32-relay-01"
```

Use the LAN IP of the machine running Mosquitto. Do not use `localhost` on the ESP32; that points to the ESP32 itself.

If you run the backend directly with `python run.py`, HTTP command polling can use port `5000`. If you run through Docker Compose, use the host-mapped backend port `5050` in `COMMAND_URL`.

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
