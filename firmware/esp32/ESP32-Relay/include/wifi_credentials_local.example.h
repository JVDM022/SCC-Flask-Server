#pragma once

#define WIFI_SSID "your-wifi-ssid"
#define EAP_IDENTITY ""
#define EAP_USERNAME ""
#define EAP_PASSWORD ""
#define MQTT_BROKER_URI "mqtt://YOUR_COMPUTER_LAN_IP:1883"
#define MQTT_BASE_TOPIC "scc"
#define MQTT_SITE_ID "site-01"
#define MQTT_RIG_ID "rig-01"
#define MQTT_DEVICE_ID "esp32-relay-01"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define COMMAND_URL "http://YOUR_COMPUTER_LAN_IP:5000/api/firmware/commands/next?device=esp32"
#define OTA_FIRMWARE_URL ""

// Arduino-over-UART OTA scaffolding. This does not flash yet; STK500v1/Optiboot support is required first.
#define ARDUINO_OTA_ENABLED 0

// Use -1 when ESP32 is not wired to Arduino RESET. If wired, set to the ESP32 GPIO that pulls RESET low.
#define ARDUINO_RESET_GPIO -1

#define ARDUINO_OTA_MAX_FIRMWARE_BYTES 32768U
#define ARDUINO_OTA_MAX_URL_LEN 320
