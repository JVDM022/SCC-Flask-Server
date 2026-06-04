#pragma once

#define WIFI_SSID "your-wifi-ssid"
#define EAP_IDENTITY ""
#define EAP_USERNAME ""
#define EAP_PASSWORD ""
#define TELEMETRY_URL "http://localhost:5000/api/telemetry"
#define COMMAND_URL "http://localhost:5000/api/command"
#define OTA_FIRMWARE_URL ""
#define IOTHUB_DEVICE_CONNECTION_STRING ""
#define NTP_SERVER "pool.ntp.org"

// Arduino-over-UART OTA scaffolding. This does not flash yet; STK500v1/Optiboot support is required first.
#define ARDUINO_OTA_ENABLED 0

// Use -1 when ESP32 is not wired to Arduino RESET. If wired, set to the ESP32 GPIO that pulls RESET low.
#define ARDUINO_RESET_GPIO -1

#define ARDUINO_OTA_MAX_FIRMWARE_BYTES 32768U
#define ARDUINO_OTA_MAX_URL_LEN 320
