#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_crt_bundle.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "relay_core.h"
#include "wifi_credentials_local.h"

#ifndef WIFI_SSID
#error "Define WIFI_SSID in wifi_credentials_local.h"
#endif
#ifndef EAP_USERNAME
#error "Define EAP_USERNAME in wifi_credentials_local.h"
#endif
#ifndef EAP_PASSWORD
#error "Define EAP_PASSWORD in wifi_credentials_local.h"
#endif
#ifndef EAP_IDENTITY
#define EAP_IDENTITY ""
#endif

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 20000U
#endif
#ifndef WIFI_RETRY_DELAY_MS
#define WIFI_RETRY_DELAY_MS 10000U
#endif

#ifndef COMMAND_URL
#define COMMAND_URL ""
#endif
#ifndef OTA_FIRMWARE_URL
#define OTA_FIRMWARE_URL ""
#endif
#ifndef OTA_CHECK_ON_BOOT
#define OTA_CHECK_ON_BOOT 0
#endif
#ifndef MQTT_BROKER_URI
#define MQTT_BROKER_URI ""
#endif
#ifndef MQTT_BASE_TOPIC
#define MQTT_BASE_TOPIC "scc"
#endif
#ifndef MQTT_SITE_ID
#define MQTT_SITE_ID "site-01"
#endif
#ifndef MQTT_RIG_ID
#define MQTT_RIG_ID "rig-01"
#endif
#ifndef MQTT_DEVICE_ID
#define MQTT_DEVICE_ID "esp32-relay-01"
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_TELEMETRY_QOS
#define MQTT_TELEMETRY_QOS 1
#endif
#ifndef MQTT_KEEPALIVE_SEC
#define MQTT_KEEPALIVE_SEC 120
#endif
#ifndef MQTT_RECYCLE_AFTER_MS
#define MQTT_RECYCLE_AFTER_MS 30000U
#endif
#ifndef ARDUINO_OTA_ENABLED
#define ARDUINO_OTA_ENABLED 0
#endif
#ifndef ARDUINO_RESET_GPIO
#define ARDUINO_RESET_GPIO -1
#endif
#ifndef ARDUINO_OTA_MAX_FIRMWARE_BYTES
#define ARDUINO_OTA_MAX_FIRMWARE_BYTES 32768U
#endif
#ifndef ARDUINO_OTA_MAX_URL_LEN
#define ARDUINO_OTA_MAX_URL_LEN 320
#endif

#define ESP32_RX2 16
#define ESP32_TX2 17
#define ARDUINO_BAUD 115200
#define UART_PORT UART_NUM_2
#define UART_RX_BUFFER_SIZE 1024

#define TELEMETRY_PERIOD_MS 1000U
#define COMMAND_PERIOD_MS 1000U

#define MAX_ARDUINO_LINE_LEN RELAY_MAX_ARDUINO_LINE_LEN
#define MAX_HTTP_RESPONSE_LEN 768
#define MAX_CMD_TYPE_LEN 24
#define MAX_OTA_URL_LEN 320
#define MAX_ARDUINO_OTA_URL_LEN ARDUINO_OTA_MAX_URL_LEN
#define MAX_MQTT_TOPIC_LEN 192
#define WIFI_CONNECTED_BIT BIT0
#define RELAY_TASK_STACK_SIZE 8192
#define RELAY_TASK_PRIORITY 5

typedef struct {
  char *buffer;
  size_t max_len;
  size_t len;
} http_response_buffer_t;

typedef enum {
  ARDUINO_OTA_START_PREPARED = 0,
  ARDUINO_OTA_START_DISABLED,
  ARDUINO_OTA_START_MISSING_URL,
  ARDUINO_OTA_START_BUSY,
  ARDUINO_OTA_START_BAD_URL,
  ARDUINO_OTA_START_NO_WIFI,
} arduino_ota_start_result_t;

static const char *TAG = "esp32-relay";
static EventGroupHandle_t s_wifi_event_group;
static bool s_wifi_connected = false;
static bool s_wifi_connecting = false;
static char s_last_arduino_line[MAX_ARDUINO_LINE_LEN] = "";
static long s_last_cmd_id_seen = -1;
static char s_http_get_response[MAX_HTTP_RESPONSE_LEN];
static char s_telemetry_payload[768];
static char s_cmd_type[MAX_CMD_TYPE_LEN];
static char s_ota_url[MAX_OTA_URL_LEN];
static char s_arduino_ota_url[MAX_ARDUINO_OTA_URL_LEN];
static char s_cmd_line[48];
static char s_mqtt_telemetry_topic[MAX_MQTT_TOPIC_LEN];
static bool s_mqtt_connected = false;
static bool s_arduino_ota_in_progress = false;
static uint32_t s_uart_rx_bytes = 0;
static uint32_t s_uart_rx_lines = 0;
static uint32_t s_last_uart_rx_log_ms = 0;
static uint32_t s_wifi_connect_started_ms = 0;
static uint32_t s_last_mqtt_disconnect_ms = 0;
static relay_arduino_snapshot_t s_arduino_snapshot = {0};
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static uint32_t now_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static bool is_http_url(const char *url) {
  return url != NULL && (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
}

static void start_wifi_connect_attempt(void) {
  s_wifi_connecting = true;
  s_wifi_connect_started_ms = now_ms();
  esp_wifi_connect();
}

static void set_arduino_ota_in_progress(bool in_progress, const char *status) {
  s_arduino_ota_in_progress = in_progress;
  ESP_LOGI(TAG, "Arduino OTA status: %s", status != NULL ? status : (in_progress ? "in_progress" : "idle"));
}

static bool arduino_reset_gpio_is_configured(void) {
  return ARDUINO_RESET_GPIO >= 0;
}

static void init_arduino_reset_gpio(void) {
#if ARDUINO_RESET_GPIO >= 0
  gpio_config_t io_conf = {
      .pin_bit_mask = 1ULL << ARDUINO_RESET_GPIO,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  ESP_ERROR_CHECK(gpio_config(&io_conf));
  ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)ARDUINO_RESET_GPIO, 1));
  ESP_LOGI(TAG, "Arduino RESET configured on GPIO%d (active low)", ARDUINO_RESET_GPIO);
#else
  ESP_LOGI(TAG, "Arduino RESET GPIO not wired/configured (ARDUINO_RESET_GPIO=%d)", ARDUINO_RESET_GPIO);
#endif
}

static bool pulse_arduino_reset(void) {
#if ARDUINO_RESET_GPIO >= 0
  ESP_LOGI(TAG, "Pulsing Arduino RESET on GPIO%d", ARDUINO_RESET_GPIO);
  ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)ARDUINO_RESET_GPIO, 0));
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)ARDUINO_RESET_GPIO, 1));
  vTaskDelay(pdMS_TO_TICKS(250));
  ESP_LOGI(TAG, "Arduino RESET pulse complete");
  return true;
#else
  ESP_LOGW(TAG, "Arduino RESET pulse skipped: ARDUINO_RESET_GPIO is not wired/configured");
  return false;
#endif
}

static arduino_ota_start_result_t prepare_arduino_ota(const char *url) {
  if (url == NULL || url[0] == '\0') {
    ESP_LOGW(TAG, "Arduino OTA command ignored: missing firmware URL");
    return ARDUINO_OTA_START_MISSING_URL;
  }
  if (ARDUINO_OTA_ENABLED == 0) {
    ESP_LOGW(TAG, "Arduino OTA command ignored: ARDUINO_OTA_ENABLED=0");
    return ARDUINO_OTA_START_DISABLED;
  }
  if (s_arduino_ota_in_progress) {
    ESP_LOGW(TAG, "Arduino OTA command ignored: another Arduino OTA operation is already in progress");
    return ARDUINO_OTA_START_BUSY;
  }
  if (!is_http_url(url)) {
    ESP_LOGW(TAG, "Arduino OTA command ignored: unsupported firmware URL scheme: %s", url);
    return ARDUINO_OTA_START_BAD_URL;
  }

  set_arduino_ota_in_progress(true, "preparing");
  relay_copy_cstr(s_arduino_ota_url, sizeof(s_arduino_ota_url), url);
  ESP_LOGI(TAG, "Arduino OTA: normal Arduino command forwarding paused");
  ESP_LOGI(TAG, "Arduino OTA: firmware URL accepted: %s", s_arduino_ota_url);
  ESP_LOGI(TAG, "Arduino OTA: max firmware size configured: %lu bytes", (unsigned long)ARDUINO_OTA_MAX_FIRMWARE_BYTES);

  if (!s_wifi_connected) {
    ESP_LOGW(TAG, "Arduino OTA: cannot prepare download path until WiFi/IP is connected");
    set_arduino_ota_in_progress(false, "failed_no_wifi");
    return ARDUINO_OTA_START_NO_WIFI;
  }

  ESP_LOGI(TAG, "Arduino OTA: download path prepared for URL fetch");
  if (arduino_reset_gpio_is_configured()) {
    (void)pulse_arduino_reset();
  } else {
    ESP_LOGW(TAG, "Arduino OTA: reset GPIO is not wired; bootloader entry must be handled by Arduino firmware or manual reset");
  }

  ESP_LOGW(TAG, "Arduino OTA: STK500v1/Optiboot programmer is not implemented/tested; firmware was not flashed");
  set_arduino_ota_in_progress(false, "prepared_not_flashed");
  ESP_LOGI(TAG, "Arduino OTA: normal Arduino command forwarding resumed");
  return ARDUINO_OTA_START_PREPARED;
}

static const char *arduino_ota_start_result_name(arduino_ota_start_result_t result) {
  switch (result) {
    case ARDUINO_OTA_START_PREPARED:
      return "prepared_not_flashed";
    case ARDUINO_OTA_START_DISABLED:
      return "disabled";
    case ARDUINO_OTA_START_MISSING_URL:
      return "missing_url";
    case ARDUINO_OTA_START_BUSY:
      return "busy";
    case ARDUINO_OTA_START_BAD_URL:
      return "bad_url";
    case ARDUINO_OTA_START_NO_WIFI:
      return "no_wifi";
    default:
      return "unknown";
  }
}

static bool mqtt_is_configured(void) {
  return MQTT_BROKER_URI[0] != '\0';
}

static bool build_mqtt_telemetry_topic(void) {
  int written = snprintf(
      s_mqtt_telemetry_topic,
      sizeof(s_mqtt_telemetry_topic),
      "%s/site/%s/rig/%s/device/%s/telemetry",
      MQTT_BASE_TOPIC,
      MQTT_SITE_ID,
      MQTT_RIG_ID,
      MQTT_DEVICE_ID);

  if (written < 0 || (size_t)written >= sizeof(s_mqtt_telemetry_topic)) {
    s_mqtt_telemetry_topic[0] = '\0';
    ESP_LOGE(TAG, "MQTT telemetry topic is too long");
    return false;
  }

  return true;
}

static void update_arduino_snapshot_from_line(const char *line) {
  relay_update_arduino_snapshot_from_line(line, &s_arduino_snapshot);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  http_response_buffer_t *response = (http_response_buffer_t *)evt->user_data;

  if (evt->event_id == HTTP_EVENT_ON_DATA && response != NULL && evt->data_len > 0) {
    size_t free_space = 0;
    size_t copy_len = 0;

    if (response->max_len > response->len) {
      free_space = response->max_len - response->len - 1;
    }
    copy_len = (size_t)evt->data_len < free_space ? (size_t)evt->data_len : free_space;

    if (copy_len > 0) {
      memcpy(response->buffer + response->len, evt->data, copy_len);
      response->len += copy_len;
      response->buffer[response->len] = '\0';
    }
  }

  return ESP_OK;
}

static bool https_get(const char *url, char *out_response, size_t out_len) {
  http_response_buffer_t response_buf;
  esp_http_client_config_t config;
  esp_http_client_handle_t client;
  esp_err_t err;
  int status_code;

  if (out_response == NULL || out_len == 0) {
    return false;
  }
  out_response[0] = '\0';

  if (!s_wifi_connected || url == NULL || url[0] == '\0') {
    return false;
  }

  response_buf.buffer = out_response;
  response_buf.max_len = out_len;
  response_buf.len = 0;

  memset(&config, 0, sizeof(config));
  config.url = url;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = 10000;
  config.event_handler = http_event_handler;
  config.user_data = &response_buf;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  client = esp_http_client_init(&config);
  if (client == NULL) {
    ESP_LOGE(TAG, "GET init failed");
    return false;
  }

  err = esp_http_client_perform(client);
  status_code = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
    return false;
  }

  ESP_LOGI(TAG, "GET %s -> %d | %s", url, status_code, out_response);
  return (status_code >= 200 && status_code < 300);
}

static void send_to_arduino(const char *cmd_line) {
  if (cmd_line == NULL || cmd_line[0] == '\0') {
    return;
  }
  if (s_arduino_ota_in_progress) {
    ESP_LOGW(TAG, "Arduino command suppressed during Arduino OTA: %s", cmd_line);
    return;
  }

  uart_write_bytes(UART_PORT, cmd_line, strlen(cmd_line));
  uart_write_bytes(UART_PORT, "\n", 1);
  ESP_LOGI(TAG, "-> Arduino: %s", cmd_line);
}

static void read_arduino_lines(void) {
  uint8_t data[64];
  int bytes_read = uart_read_bytes(UART_PORT, data, sizeof(data), pdMS_TO_TICKS(10));
  static char line_buf[MAX_ARDUINO_LINE_LEN];
  static size_t line_len = 0;
  size_t i;
  char preview[33];
  size_t preview_len;

  if (bytes_read <= 0) {
    return;
  }

  s_uart_rx_bytes += (uint32_t)bytes_read;
  if ((now_ms() - s_last_uart_rx_log_ms) >= 2000U) {
    preview_len = (size_t)bytes_read < (sizeof(preview) - 1U) ? (size_t)bytes_read : (sizeof(preview) - 1U);
    for (i = 0; i < preview_len; i++) {
      preview[i] = isprint((unsigned char)data[i]) ? (char)data[i] : '.';
    }
    preview[preview_len] = '\0';
    ESP_LOGI(TAG, "UART2 rx: %d bytes (total=%lu, lines=%lu), preview=\"%s\"", bytes_read,
             (unsigned long)s_uart_rx_bytes, (unsigned long)s_uart_rx_lines, preview);
    s_last_uart_rx_log_ms = now_ms();
  }

  for (i = 0; i < (size_t)bytes_read; i++) {
    char c = (char)data[i];

    if (c == '\r' || c == '\n') {
      line_buf[line_len] = '\0';
      relay_trim_inplace(line_buf);
      if (line_buf[0] != '\0') {
        relay_copy_cstr(s_last_arduino_line, sizeof(s_last_arduino_line), line_buf);
        update_arduino_snapshot_from_line(s_last_arduino_line);
        s_uart_rx_lines++;
        ESP_LOGI(TAG, "<- Arduino: %s", s_last_arduino_line);
      }
      line_len = 0;
      continue;
    }

    if (line_len < sizeof(line_buf) - 1) {
      line_buf[line_len++] = c;
    } else {
      line_len = 0;
    }
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t event_client = event != NULL ? event->client : NULL;

  (void)handler_args;
  (void)base;

  if (event_client == NULL || event_client != s_mqtt_client) {
    return;
  }

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      s_mqtt_connected = true;
      s_last_mqtt_disconnect_ms = 0;
      ESP_LOGI(TAG, "Local MQTT connected");
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGD(TAG, "MQTT data received on an unexpected subscription");
      break;
    case MQTT_EVENT_DISCONNECTED:
      s_mqtt_connected = false;
      s_last_mqtt_disconnect_ms = now_ms();
      ESP_LOGW(TAG, "Local MQTT disconnected");
      break;
    case MQTT_EVENT_ERROR:
      s_mqtt_connected = false;
      s_last_mqtt_disconnect_ms = now_ms();
      ESP_LOGW(TAG, "Local MQTT error");
      if (event != NULL && event->error_handle != NULL) {
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
          ESP_LOGW(TAG, "MQTT connection refused: %d", event->error_handle->connect_return_code);
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
          ESP_LOGW(TAG, "MQTT transport err=0x%x tls=0x%x errno=%d", event->error_handle->esp_tls_last_esp_err,
                   event->error_handle->esp_tls_stack_err, event->error_handle->esp_transport_sock_errno);
        }
      }
      break;
    default:
      break;
  }
}

static void stop_mqtt_client(void) {
  esp_mqtt_client_handle_t client = s_mqtt_client;

  if (s_mqtt_client == NULL) {
    return;
  }

  s_mqtt_client = NULL;
  s_mqtt_connected = false;
  s_last_mqtt_disconnect_ms = 0;
  esp_mqtt_client_stop(client);
  esp_mqtt_client_destroy(client);
}

static bool start_mqtt_client(void) {
  esp_mqtt_client_config_t mqtt_cfg = {0};

  if (!mqtt_is_configured() || !build_mqtt_telemetry_topic()) {
    return false;
  }

  mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
  mqtt_cfg.credentials.client_id = MQTT_DEVICE_ID;
  if (MQTT_USERNAME[0] != '\0') {
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
  }
  mqtt_cfg.session.keepalive = MQTT_KEEPALIVE_SEC;
  mqtt_cfg.session.disable_clean_session = false;
  mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
  mqtt_cfg.network.timeout_ms = 10000;
  mqtt_cfg.network.reconnect_timeout_ms = 5000;
  mqtt_cfg.network.disable_auto_reconnect = false;
  mqtt_cfg.buffer.size = 1024;
  mqtt_cfg.buffer.out_size = 1024;
  mqtt_cfg.task.stack_size = 6144;

  s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (s_mqtt_client == NULL) {
    ESP_LOGE(TAG, "Failed to create local MQTT client");
    return false;
  }

  esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  if (esp_mqtt_client_start(s_mqtt_client) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start local MQTT client");
    esp_mqtt_client_destroy(s_mqtt_client);
    s_mqtt_client = NULL;
    return false;
  }

  s_mqtt_connected = false;
  s_last_mqtt_disconnect_ms = now_ms();
  ESP_LOGI(TAG, "Starting local MQTT client: broker=%s topic=%s", MQTT_BROKER_URI, s_mqtt_telemetry_topic);
  return true;
}

static bool ensure_mqtt_client(void) {
  if (!mqtt_is_configured()) {
    return false;
  }
  if (!s_wifi_connected) {
    if (s_mqtt_client != NULL) {
      stop_mqtt_client();
    }
    return false;
  }

  if (s_mqtt_client == NULL) {
    return start_mqtt_client();
  }

  if (!s_mqtt_connected && s_last_mqtt_disconnect_ms != 0U) {
    uint32_t disconnected_for_ms = now_ms() - s_last_mqtt_disconnect_ms;

    if (disconnected_for_ms >= MQTT_RECYCLE_AFTER_MS) {
      ESP_LOGW(TAG, "Local MQTT still disconnected after %lu ms. Recycling client.",
               (unsigned long)disconnected_for_ms);
      stop_mqtt_client();
      return start_mqtt_client();
    }
  }

  return true;
}

static bool publish_telemetry_mqtt(const char *payload) {
  int msg_id;

  if (payload == NULL || payload[0] == '\0') {
    return false;
  }
  if (!ensure_mqtt_client() || !s_mqtt_connected) {
    return false;
  }

  msg_id = esp_mqtt_client_publish(s_mqtt_client, s_mqtt_telemetry_topic, payload, 0, MQTT_TELEMETRY_QOS, 0);
  if (msg_id < 0) {
    ESP_LOGW(TAG, "Failed to queue local MQTT telemetry publish");
    return false;
  }

  ESP_LOGI(TAG, "Local MQTT telemetry queued: topic=%s msg_id=%d", s_mqtt_telemetry_topic, msg_id);
  return true;
}

static void post_telemetry(void) {
  uint32_t ts = now_ms();
  bool telemetry_sent = false;
  const relay_arduino_snapshot_t *sample = &s_arduino_snapshot;

  if (!sample->have_sample) {
    ESP_LOGW(TAG, "Telemetry skipped: no valid Arduino CSV sample yet");
    return;
  }

  snprintf(
      s_telemetry_payload,
      sizeof(s_telemetry_payload),
      "{\"event\":%d,\"ms\":%lu,\"temp_c\":%.2f,\"adc\":%d,\"dtemp_c_per_s\":%.4f,"
      "\"setpoint_c\":%.2f,\"mode\":%d,\"heater_pwm\":%d,\"heating\":%d,"
      "\"heater_lockout\":%d,\"pump_enabled\":%d,\"pump_allowed\":%d,\"pump_on\":%d,"
      "\"motor_pwm\":%d,\"motor_on_ms\":%lu,\"motor_period_ms\":%lu,"
      "\"temp_before_pump_c\":%.2f,\"min_temp_after_pump_c\":%.2f,"
      "\"last_pump_drop_c\":%.2f,\"recovery_time_s\":%.2f,\"manual_kill\":%d,"
      "\"hard_kill\":%d,\"uptime_s\":%lu,\"ts\":%lu}",
      sample->event,
      (unsigned long)sample->ms,
      sample->temp_c,
      sample->adc,
      sample->dtemp_c_per_s,
      sample->setpoint_c,
      sample->mode,
      sample->heater_pwm,
      sample->heating,
      sample->heater_lockout,
      sample->pump_enabled,
      sample->pump_allowed,
      sample->pump_on,
      sample->motor_pwm,
      (unsigned long)sample->motor_on_ms,
      (unsigned long)sample->motor_period_ms,
      sample->temp_before_pump_c,
      sample->min_temp_after_pump_c,
      sample->last_pump_drop_c,
      sample->recovery_time_s,
      sample->manual_kill,
      sample->hard_kill,
      (unsigned long)sample->uptime_s,
      (unsigned long)ts);

  ESP_LOGI(TAG, "Telemetry payload: %s", s_telemetry_payload);
  if (mqtt_is_configured()) {
    if (!s_wifi_connected) {
      ESP_LOGI(TAG, "Telemetry deferred: waiting for WiFi/IP");
    } else {
      telemetry_sent = publish_telemetry_mqtt(s_telemetry_payload);
    }
    if (!telemetry_sent && s_wifi_connected) {
      if (!s_mqtt_connected) {
        ESP_LOGI(TAG, "Telemetry deferred: waiting for local MQTT connection");
      } else {
        ESP_LOGW(TAG, "Local MQTT telemetry publish did not complete");
      }
    }
  }

  if (!telemetry_sent && !mqtt_is_configured()) {
    ESP_LOGW(TAG, "Telemetry skipped: MQTT_BROKER_URI is not configured");
  }
}

static void perform_ota_update(const char *url) {
  esp_http_client_config_t http_config;
  esp_https_ota_config_t ota_config;
  esp_err_t err;

  if (url == NULL || url[0] == '\0') {
    ESP_LOGW(TAG, "OTA skipped: no URL configured");
    return;
  }
  if (!s_wifi_connected) {
    ESP_LOGW(TAG, "OTA skipped: WiFi is not connected");
    return;
  }

  memset(&http_config, 0, sizeof(http_config));
  http_config.url = url;
  http_config.timeout_ms = 20000;
  http_config.crt_bundle_attach = esp_crt_bundle_attach;

  memset(&ota_config, 0, sizeof(ota_config));
  ota_config.http_config = &http_config;

  ESP_LOGI(TAG, "Starting OTA update: %s", url);
  err = esp_https_ota(&ota_config);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "OTA update successful. Rebooting...");
    esp_restart();
    return;
  }

  ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
}

static void poll_command_and_forward(void) {
  long cmd_id;
  long value;
  const char *selected_ota_url;
  relay_backend_command_type_t command_type;
  arduino_ota_start_result_t arduino_ota_result;

  s_http_get_response[0] = '\0';
  s_cmd_type[0] = '\0';
  s_ota_url[0] = '\0';

  if (!https_get(COMMAND_URL, s_http_get_response, sizeof(s_http_get_response))) {
    return;
  }

  relay_trim_inplace(s_http_get_response);
  if (s_http_get_response[0] == '\0') {
    return;
  }

  cmd_id = relay_extract_long_json(s_http_get_response, "cmdId");
  value = relay_extract_long_json(s_http_get_response, "value");
  if (cmd_id <= 0 || !relay_extract_string_json(s_http_get_response, "type", s_cmd_type, sizeof(s_cmd_type)) || s_cmd_type[0] == '\0') {
    return;
  }
  if (!relay_parse_backend_command_type(s_cmd_type, &command_type)) {
    ESP_LOGW(TAG, "Unknown command type: %s", s_cmd_type);
    return;
  }

  if (cmd_id == s_last_cmd_id_seen) {
    return;
  }
  s_last_cmd_id_seen = cmd_id;

  switch (command_type) {
    case RELAY_BACKEND_COMMAND_KILL:
      snprintf(s_cmd_line, sizeof(s_cmd_line), "KILL %ld", value);
      send_to_arduino(s_cmd_line);
      break;
    case RELAY_BACKEND_COMMAND_SET_ON:
      snprintf(s_cmd_line, sizeof(s_cmd_line), "SET_ON %ld", value);
      send_to_arduino(s_cmd_line);
      break;
    case RELAY_BACKEND_COMMAND_ESP32_OTA:
      selected_ota_url = OTA_FIRMWARE_URL;
      s_ota_url[0] = '\0';
      if (relay_extract_string_json(s_http_get_response, "url", s_ota_url, sizeof(s_ota_url)) && s_ota_url[0] != '\0') {
        selected_ota_url = s_ota_url;
      }

      if (selected_ota_url == NULL || selected_ota_url[0] == '\0') {
        ESP_LOGW(TAG, "OTA command ignored: missing firmware URL");
        return;
      }

      perform_ota_update(selected_ota_url);
      break;
    case RELAY_BACKEND_COMMAND_ARDUINO_OTA:
      s_arduino_ota_url[0] = '\0';
      (void)relay_extract_string_json(s_http_get_response, "url", s_arduino_ota_url, sizeof(s_arduino_ota_url));
      arduino_ota_result = prepare_arduino_ota(s_arduino_ota_url);
      ESP_LOGI(TAG, "Arduino OTA command result: %s", arduino_ota_start_result_name(arduino_ota_result));
      break;
    case RELAY_BACKEND_COMMAND_UNKNOWN:
    default:
      ESP_LOGW(TAG, "Unknown command type: %s", s_cmd_type);
      break;
  }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    start_wifi_connect_attempt();
    return;
  }

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    s_wifi_connected = false;
    s_wifi_connecting = false;
    s_wifi_connect_started_ms = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    ESP_LOGW(TAG, "WiFi disconnected");
    return;
  }

  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    wifi_ap_record_t ap_info;

    s_wifi_connected = true;
    s_wifi_connecting = false;
    s_wifi_connect_started_ms = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    ESP_LOGI(TAG, "WiFi connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      ESP_LOGI(TAG, "RSSI: %d dBm", ap_info.rssi);
    }
  }
}

static void init_uart(void) {
  uart_config_t uart_config = {
      .baud_rate = ARDUINO_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUFFER_SIZE, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT, ESP32_TX2, ESP32_RX2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_LOGI(TAG, "UART2 configured: TX=%d RX=%d baud=%d", ESP32_TX2, ESP32_RX2, ARDUINO_BAUD);
}

static void init_wifi_enterprise(void) {
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t wifi_config = {0};
  const char *identity = EAP_IDENTITY;
  EventBits_t bits;

  if (identity[0] == '\0') {
    identity = EAP_USERNAME;
  }

  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

  relay_copy_cstr((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), WIFI_SSID);
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  esp_eap_client_clear_ca_cert();
  ESP_ERROR_CHECK(esp_eap_client_set_identity((const uint8_t *)identity, strlen(identity)));
  ESP_ERROR_CHECK(esp_eap_client_set_username((const uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME)));
  ESP_ERROR_CHECK(esp_eap_client_set_password((const uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)));
  ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
  s_wifi_connecting = true;
  s_wifi_connect_started_ms = now_ms();

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Connecting to WPA2-Enterprise WiFi: %s", WIFI_SSID);

  bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE,
                             pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
  if ((bits & WIFI_CONNECTED_BIT) == 0U) {
    ESP_LOGW(TAG, "WiFi connect timeout after %lu ms", (unsigned long)WIFI_CONNECT_TIMEOUT_MS);
  }
}


static void relay_task(void *arg) {
  uint32_t last_telemetry_ms = 0;
  uint32_t last_command_poll_ms = 0;
  uint32_t last_mqtt_service_ms = 0;
  uint32_t last_wifi_retry_ms = 0;
  uint32_t last_uart_idle_log_ms = 0;

  (void)arg;

  for (;;) {
    uint32_t now = now_ms();

    if (!s_wifi_connected && s_mqtt_client != NULL) {
      stop_mqtt_client();
    }

    if (!s_wifi_connected && !s_wifi_connecting && (now - last_wifi_retry_ms) >= WIFI_RETRY_DELAY_MS) {
      last_wifi_retry_ms = now;
      ESP_LOGW(TAG, "WiFi disconnected. Reconnecting...");
      start_wifi_connect_attempt();
    }

    read_arduino_lines();

    if ((now - last_uart_idle_log_ms) >= 5000U) {
      last_uart_idle_log_ms = now;
      if (s_uart_rx_bytes == 0U) {
        ESP_LOGW(TAG, "UART2 idle: no bytes received yet on RX=%d", ESP32_RX2);
      }
    }

    if ((now - last_mqtt_service_ms) >= 1000U) {
      last_mqtt_service_ms = now;
      if (mqtt_is_configured()) {
        ensure_mqtt_client();
      }
    }

    if ((now - last_telemetry_ms) >= TELEMETRY_PERIOD_MS) {
      last_telemetry_ms = now;
      if (!s_wifi_connected && mqtt_is_configured()) {
        // Keep consuming UART locally, but wait for WiFi/IP before attempting upstream telemetry.
      } else {
        post_telemetry();
      }
    }

    if ((now - last_command_poll_ms) >= COMMAND_PERIOD_MS) {
      last_command_poll_ms = now;
      poll_command_and_forward();
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void app_main(void) {
  esp_err_t ret;

  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  init_uart();
  init_arduino_reset_gpio();
  init_wifi_enterprise();

  if (OTA_CHECK_ON_BOOT != 0) {
    perform_ota_update(OTA_FIRMWARE_URL);
  }

  ESP_LOGI(TAG, "Ready: UART telemetry bridge + local MQTT telemetry + HTTP commands + OTA");

  xTaskCreatePinnedToCore(relay_task, "relay_task", RELAY_TASK_STACK_SIZE, NULL, RELAY_TASK_PRIORITY, NULL, 1);
}
