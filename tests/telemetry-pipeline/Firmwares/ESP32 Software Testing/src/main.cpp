#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "relay_core.h"
}

#if __has_include("test_config.h")
#include "test_config.h"
#endif

#ifndef TEST_WIFI_SSID
#define TEST_WIFI_SSID ""
#endif
#ifndef TEST_WIFI_PASSWORD
#define TEST_WIFI_PASSWORD ""
#endif
#ifndef TEST_FLASK_TELEMETRY_URL
#define TEST_FLASK_TELEMETRY_URL ""
#endif
#ifndef TEST_API_WRITE_KEY
#define TEST_API_WRITE_KEY ""
#endif
#ifndef TEST_UART_RX_PIN
#define TEST_UART_RX_PIN 16
#endif
#ifndef TEST_UART_TX_PIN
#define TEST_UART_TX_PIN 17
#endif
#ifndef TEST_ARDUINO_BAUD
#define TEST_ARDUINO_BAUD 115200
#endif

static relay_arduino_snapshot_t snapshot;
static char lineBuffer[RELAY_MAX_ARDUINO_LINE_LEN];
static size_t lineLength = 0;
static uint32_t lastWifiAttemptMs = 0;

static bool startsWithTelemetryHeader(const char *line) {
  return strncmp(line, "event,ms,temp_c", 15) == 0;
}

static bool wifiReady() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  const uint32_t now = millis();
  if (lastWifiAttemptMs == 0 || now - lastWifiAttemptMs >= 10000U) {
    lastWifiAttemptMs = now;
    Serial.println("WIFI_CONNECTING");
    WiFi.mode(WIFI_STA);
    WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  }
  return false;
}

static bool configReady() {
  return strlen(TEST_WIFI_SSID) > 0 && strlen(TEST_FLASK_TELEMETRY_URL) > 0;
}

static void buildTelemetryJson(char *out, size_t outLen, const relay_arduino_snapshot_t &sample) {
  snprintf(
      out,
      outLen,
      "{\"event\":%d,\"ms\":%lu,\"temp_c\":%.2f,\"adc\":%d,\"dtemp_c_per_s\":%.4f,"
      "\"setpoint_c\":%.2f,\"mode\":%d,\"heater_pwm\":%d,\"heating\":%d,"
      "\"heater_lockout\":%d,\"pump_enabled\":%d,\"pump_allowed\":%d,\"pump_on\":%d,"
      "\"motor_pwm\":%d,\"motor_on_ms\":%lu,\"motor_period_ms\":%lu,"
      "\"temp_before_pump_c\":%.2f,\"min_temp_after_pump_c\":%.2f,"
      "\"last_pump_drop_c\":%.2f,\"recovery_time_s\":%.2f,\"manual_kill\":%d,"
      "\"hard_kill\":%d,\"uptime_s\":%lu}",
      sample.event,
      static_cast<unsigned long>(sample.ms),
      sample.temp_c,
      sample.adc,
      sample.dtemp_c_per_s,
      sample.setpoint_c,
      sample.mode,
      sample.heater_pwm,
      sample.heating,
      sample.heater_lockout,
      sample.pump_enabled,
      sample.pump_allowed,
      sample.pump_on,
      sample.motor_pwm,
      static_cast<unsigned long>(sample.motor_on_ms),
      static_cast<unsigned long>(sample.motor_period_ms),
      sample.temp_before_pump_c,
      sample.min_temp_after_pump_c,
      sample.last_pump_drop_c,
      sample.recovery_time_s,
      sample.manual_kill,
      sample.hard_kill,
      static_cast<unsigned long>(sample.uptime_s));
}

static void postTelemetry(const char *sourceCsv) {
  char payload[768];
  HTTPClient http;

  if (!configReady()) {
    Serial.println("CONFIG_MISSING: copy include/test_config.example.h to include/test_config.h and fill it in");
    return;
  }
  if (!wifiReady()) {
    Serial.println("POST_DEFERRED: wifi_not_connected");
    return;
  }

  buildTelemetryJson(payload, sizeof(payload), snapshot);
  Serial.print("SOURCE_CSV:");
  Serial.println(sourceCsv);
  Serial.print("POST_JSON:");
  Serial.println(payload);

  http.begin(TEST_FLASK_TELEMETRY_URL);
  http.addHeader("Content-Type", "application/json");
  if (strlen(TEST_API_WRITE_KEY) > 0) {
    http.addHeader("X-API-Key", TEST_API_WRITE_KEY);
  }

  const int status = http.POST(reinterpret_cast<uint8_t *>(payload), strlen(payload));
  const String body = http.getString();
  http.end();

  if (status >= 200 && status < 300) {
    Serial.print("POST_OK:");
  } else {
    Serial.print("POST_ERROR:");
  }
  Serial.print(status);
  Serial.print(":");
  Serial.println(body);
}

static void handleLine(char *line) {
  relay_trim_inplace(line);
  if (line[0] == '\0') {
    return;
  }
  if (startsWithTelemetryHeader(line)) {
    Serial.print("HEADER_SEEN:");
    Serial.println(line);
    return;
  }

  relay_arduino_snapshot_t before = snapshot;
  relay_update_arduino_snapshot_from_line(line, &snapshot);
  if (!snapshot.have_sample || (before.have_sample && before.ms == snapshot.ms)) {
    Serial.print("PARSE_SKIPPED:");
    Serial.println(line);
    return;
  }

  postTelemetry(line);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(TEST_ARDUINO_BAUD, SERIAL_8N1, TEST_UART_RX_PIN, TEST_UART_TX_PIN);
  memset(&snapshot, 0, sizeof(snapshot));
  Serial.println("ESP32_FLASK_RELAY_TEST_READY");
  Serial.printf("UART2_RX_GPIO:%d UART2_TX_GPIO:%d BAUD:%d\n", TEST_UART_RX_PIN, TEST_UART_TX_PIN, TEST_ARDUINO_BAUD);
}

void loop() {
  if (configReady()) {
    (void)wifiReady();
  }

  while (Serial2.available() > 0) {
    const char c = static_cast<char>(Serial2.read());
    if (c == '\r' || c == '\n') {
      lineBuffer[lineLength] = '\0';
      handleLine(lineBuffer);
      lineLength = 0;
      continue;
    }

    if (lineLength < sizeof(lineBuffer) - 1U) {
      lineBuffer[lineLength++] = c;
    } else {
      lineLength = 0;
      Serial.println("LINE_OVERFLOW");
    }
  }
}
