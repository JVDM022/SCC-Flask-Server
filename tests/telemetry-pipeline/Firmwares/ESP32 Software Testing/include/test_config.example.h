#pragma once

#define TEST_WIFI_SSID "your-wifi-ssid"
#define TEST_WIFI_PASSWORD "your-wifi-password"

// Use your computer's LAN IP, not localhost, because this runs on the ESP32.
#define TEST_FLASK_TELEMETRY_URL "http://192.168.1.100:5050/api/telemetry"

// Must match the backend API_WRITE_KEY environment variable.
#define TEST_API_WRITE_KEY "test-write-key"
