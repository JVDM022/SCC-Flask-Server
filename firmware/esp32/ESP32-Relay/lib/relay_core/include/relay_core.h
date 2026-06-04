#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RELAY_MAX_ARDUINO_LINE_LEN 256
#define RELAY_MAX_IOTHUB_TOPIC_LEN 160
#define RELAY_MAX_IOTHUB_METHOD_PAYLOAD_LEN 192

typedef struct {
  bool have_sample;
  int event;
  uint32_t ms;
  float temp_c;
  int adc;
  float dtemp_c_per_s;
  float setpoint_c;
  int mode;
  int heater_pwm;
  int heating;
  int heater_lockout;
  int pump_enabled;
  int pump_allowed;
  int pump_on;
  int motor_pwm;
  uint32_t motor_on_ms;
  uint32_t motor_period_ms;
  float temp_before_pump_c;
  float min_temp_after_pump_c;
  float last_pump_drop_c;
  float recovery_time_s;
  int manual_kill;
  int hard_kill;
  uint32_t uptime_s;
} relay_arduino_snapshot_t;

typedef enum {
  RELAY_BACKEND_COMMAND_UNKNOWN = 0,
  RELAY_BACKEND_COMMAND_KILL,
  RELAY_BACKEND_COMMAND_SET_ON,
  RELAY_BACKEND_COMMAND_ESP32_OTA,
  RELAY_BACKEND_COMMAND_ARDUINO_OTA,
} relay_backend_command_type_t;

void relay_copy_cstr(char *dest, size_t dest_len, const char *src);
void relay_copy_lower_ascii(char *dest, size_t dest_len, const char *src);
void relay_trim_inplace(char *s);

bool relay_url_encode(const char *src, char *dest, size_t dest_len);
bool relay_extract_conn_string_value(const char *conn, const char *key, char *out, size_t out_len);

void relay_update_arduino_snapshot_from_line(const char *line, relay_arduino_snapshot_t *snapshot);
bool relay_parse_backend_command_type(const char *type, relay_backend_command_type_t *out);

long relay_extract_long_json(const char *json, const char *key);
bool relay_extract_string_json(const char *json, const char *key, char *out, size_t out_len);
bool relay_extract_long_json_value(const char *json, const char *key, long *out);

bool relay_parse_direct_method_long_value(const char *payload, long *out);
bool relay_parse_iothub_direct_method_topic(
    const char *topic,
    size_t topic_len,
    char *method_name,
    size_t method_name_len,
    char *request_id,
    size_t request_id_len);
