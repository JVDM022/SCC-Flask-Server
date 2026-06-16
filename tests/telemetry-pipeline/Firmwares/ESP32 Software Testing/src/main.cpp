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
#define TEST_UART_RX_PIN 3
#endif
#ifndef TEST_UART_TX_PIN
#define TEST_UART_TX_PIN 1
#endif
#ifndef TEST_ARDUINO_BAUD
#define TEST_ARDUINO_BAUD 115200
#endif
#ifndef TEST_WIFI_SCAN_ON_BOOT
#define TEST_WIFI_SCAN_ON_BOOT 1
#endif

static relay_arduino_snapshot_t snapshot;
static char lineBuffer[RELAY_MAX_ARDUINO_LINE_LEN];
static size_t lineLength = 0;
static bool discardingLine = false;
static bool wifiWasConnected = false;
static uint32_t lastWifiAttemptMs = 0;
static uint32_t lastWifiStatusMs = 0;
static uint32_t ignoredByteCount = 0;
static uint32_t resyncDropCount = 0;

static bool wifiConfigured() {
  return strlen(TEST_WIFI_SSID) > 0 && strlen(TEST_FLASK_TELEMETRY_URL) > 0;
}

static bool apiKeyConfigured() {
  return strlen(TEST_API_WRITE_KEY) > 0;
}

static const char *wifiStatusName(wl_status_t status) {
  switch (status) {
  case WL_NO_SHIELD:
    return "NO_SHIELD";
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "NO_SSID";
  case WL_SCAN_COMPLETED:
    return "SCAN_COMPLETED";
  case WL_CONNECTED:
    return "CONNECTED";
  case WL_CONNECT_FAILED:
    return "CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

static const char *wifiAuthName(wifi_auth_mode_t authMode) {
  switch (authMode) {
  case WIFI_AUTH_OPEN:
    return "OPEN";
  case WIFI_AUTH_WEP:
    return "WEP";
  case WIFI_AUTH_WPA_PSK:
    return "WPA";
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2";
  case WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA_WPA2";
  case WIFI_AUTH_WPA2_ENTERPRISE:
    return "WPA2_ENTERPRISE";
  case WIFI_AUTH_WPA3_PSK:
    return "WPA3";
  case WIFI_AUTH_WPA2_WPA3_PSK:
    return "WPA2_WPA3";
  default:
    return "UNKNOWN";
  }
}

static void printWifiStatus(const char *prefix) {
  const wl_status_t status = WiFi.status();
  Serial.print(prefix);
  Serial.print(wifiStatusName(status));
  Serial.print(":");
  Serial.println(static_cast<int>(status));
}

static void scanWifiNetworks() {
  Serial.println("WIFI_SCAN_START");
  const int count = WiFi.scanNetworks(false, true);
  if (count < 0) {
    Serial.print("WIFI_SCAN_ERROR:");
    Serial.println(count);
    return;
  }

  Serial.print("WIFI_SCAN_COUNT:");
  Serial.println(count);
  for (int index = 0; index < count; ++index) {
    Serial.print("WIFI_SCAN_RESULT:");
    Serial.print(WiFi.SSID(index));
    Serial.print(",RSSI:");
    Serial.print(WiFi.RSSI(index));
    Serial.print(",AUTH:");
    Serial.println(wifiAuthName(WiFi.encryptionType(index)));
  }
  WiFi.scanDelete();
}

static bool wifiReady() {
  if (!wifiConfigured()) {
    return false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.print("WIFI_CONNECTED:");
      Serial.print(WiFi.localIP());
      Serial.print(",RSSI:");
      Serial.println(WiFi.RSSI());
    }
    return true;
  }

  wifiWasConnected = false;
  const uint32_t now = millis();
  if (lastWifiStatusMs == 0 || now - lastWifiStatusMs >= 5000U) {
    lastWifiStatusMs = now;
    printWifiStatus("WIFI_STATUS:");
  }

  if (lastWifiAttemptMs == 0 || now - lastWifiAttemptMs >= 15000U) {
    lastWifiAttemptMs = now;
    Serial.print("WIFI_CONNECTING:");
    Serial.println(TEST_WIFI_SSID);
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  }
  return false;
}

static bool startsWithTelemetryHeader(const char *line) {
  return strncmp(line, "event,ms,temp_c", 15) == 0;
}

static bool isAsciiLineByte(char c) {
  return c >= 32 && c <= 126;
}

static bool isValidLineStart(char c) {
  return (c >= '0' && c <= '5') || c == 'e';
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

static void sendTelemetry(const char *sourceCsv) {
  char payload[768];
  buildTelemetryJson(payload, sizeof(payload), snapshot);

  Serial.print("SOURCE_CSV:");
  Serial.println(sourceCsv);
  Serial.print("POST_JSON:");
  Serial.println(payload);

  if (!wifiConfigured()) {
    Serial.println("HTTP_DISABLED:wifi_or_url_not_configured");
    return;
  }

  if (!wifiReady()) {
    Serial.println("POST_DEFERRED:wifi_not_connected");
    return;
  }

  HTTPClient http;
  http.begin(TEST_FLASK_TELEMETRY_URL);
  http.addHeader("Content-Type", "application/json");
  if (apiKeyConfigured()) {
    http.addHeader("X-API-Key", TEST_API_WRITE_KEY);
  }

  const int status = http.POST(reinterpret_cast<uint8_t *>(payload), strlen(payload));
  const String body = http.getString();
  http.end();

  Serial.print(status >= 200 && status < 300 ? "POST_OK:" : "POST_ERROR:");
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

  sendTelemetry(line);
}

static void handleUartByte(char c) {
  if (c == '\r' || c == '\n') {
    if (!discardingLine) {
      lineBuffer[lineLength] = '\0';
      handleLine(lineBuffer);
    }
    lineLength = 0;
    discardingLine = false;
    return;
  }

  if (discardingLine) {
    return;
  }

  if (!isAsciiLineByte(c)) {
    ignoredByteCount++;
    if (ignoredByteCount <= 8 || ignoredByteCount % 50 == 0) {
      Serial.print("UART_NON_ASCII_IGNORED:");
      Serial.println(ignoredByteCount);
    }
    return;
  }

  if (lineLength == 0 && !isValidLineStart(c)) {
    resyncDropCount++;
    if (resyncDropCount <= 8 || resyncDropCount % 50 == 0) {
      Serial.print("UART_RESYNC_DROP:");
      Serial.println(resyncDropCount);
    }
    return;
  }

  if (lineLength < sizeof(lineBuffer) - 1U) {
    lineBuffer[lineLength++] = c;
    return;
  }

  Serial.println("LINE_OVERFLOW:discarding_until_newline");
  lineLength = 0;
  discardingLine = true;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(TEST_ARDUINO_BAUD, SERIAL_8N1, TEST_UART_RX_PIN, TEST_UART_TX_PIN);
  memset(&snapshot, 0, sizeof(snapshot));

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  Serial.println("ESP32_FLASK_RELAY_TEST_READY");
  Serial.printf("UART2_RX_GPIO:%d UART2_TX_GPIO:%d BAUD:%d\n", TEST_UART_RX_PIN, TEST_UART_TX_PIN, TEST_ARDUINO_BAUD);
  Serial.print("WIFI_SSID:");
  Serial.println(strlen(TEST_WIFI_SSID) > 0 ? TEST_WIFI_SSID : "<disabled>");
  Serial.print("HTTP_URL:");
  Serial.println(strlen(TEST_FLASK_TELEMETRY_URL) > 0 ? TEST_FLASK_TELEMETRY_URL : "<disabled>");
  Serial.print("API_KEY_SET:");
  Serial.println(apiKeyConfigured() ? "yes" : "no");

#if TEST_WIFI_SCAN_ON_BOOT
  if (strlen(TEST_WIFI_SSID) > 0) {
    scanWifiNetworks();
  }
#endif
}

void loop() {
  (void)wifiReady();

  while (Serial2.available() > 0) {
    handleUartByte(static_cast<char>(Serial2.read()));
  }
}
