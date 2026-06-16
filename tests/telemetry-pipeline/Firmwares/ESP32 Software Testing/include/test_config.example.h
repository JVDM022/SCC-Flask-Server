#pragma once

#define TEST_WIFI_SSID "your-2.4ghz-wifi-ssid"
#define TEST_WIFI_PASSWORD "your-wifi-password"

// Use the Flask server LAN IP, not localhost, because this runs on the ESP32.
// Leave TEST_WIFI_SSID or TEST_FLASK_TELEMETRY_URL empty to disable direct HTTP
// posting and use the USB serial bridge instead.
#define TEST_FLASK_TELEMETRY_URL "http://192.168.1.100:5050/api/telemetry"

// Must match backend API_WRITE_KEY when Flask write authentication is enabled.
// Leave empty only when Flask API_WRITE_KEY is empty or when using only USB logs.
#define TEST_API_WRITE_KEY "test-write-key"

// Set to 0 after Wi-Fi is stable if boot scans are too noisy.
#define TEST_WIFI_SCAN_ON_BOOT 1
