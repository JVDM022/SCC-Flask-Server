#include "relay_core.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void relay_copy_cstr(char *dest, size_t dest_len, const char *src) {
  if (dest_len == 0) {
    return;
  }
  if (src == NULL) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, dest_len - 1);
  dest[dest_len - 1] = '\0';
}

void relay_trim_inplace(char *s) {
  size_t len;
  size_t i;
  size_t start = 0;

  if (s == NULL) {
    return;
  }

  len = strlen(s);
  while (start < len && isspace((unsigned char)s[start])) {
    start++;
  }
  if (start > 0) {
    memmove(s, s + start, len - start + 1);
  }

  len = strlen(s);
  for (i = len; i > 0; i--) {
    if (!isspace((unsigned char)s[i - 1])) {
      break;
    }
    s[i - 1] = '\0';
  }
}

static bool relay_read_csv_field(const char **cursor, char *out, size_t out_len, bool *has_more) {
  const char *start;
  const char *end;
  size_t len;

  if (cursor == NULL || *cursor == NULL || out == NULL || out_len == 0 || has_more == NULL) {
    return false;
  }

  start = *cursor;
  end = start;
  while (*end != '\0' && *end != ',' && *end != '\r' && *end != '\n') {
    end++;
  }

  while (start < end && isspace((unsigned char)*start)) {
    start++;
  }
  while (end > start && isspace((unsigned char)end[-1])) {
    end--;
  }

  len = (size_t)(end - start);
  if (len >= out_len) {
    out[0] = '\0';
    return false;
  }

  memcpy(out, start, len);
  out[len] = '\0';

  if (**cursor != '\0') {
    while (*end != '\0' && *end != ',' && *end != '\r' && *end != '\n') {
      end++;
    }
  }
  if (*end == ',') {
    *cursor = end + 1;
    *has_more = true;
  } else {
    *cursor = end;
    *has_more = false;
  }
  return true;
}

static bool relay_parse_csv_float_token(const char *token, float *out) {
  char *end_ptr;

  if (token == NULL || token[0] == '\0' || out == NULL) {
    return false;
  }

  *out = strtof(token, &end_ptr);
  return end_ptr != token && *end_ptr == '\0';
}

static bool relay_parse_csv_int_token(const char *token, int *out) {
  long value;
  char *end_ptr;

  if (token == NULL || token[0] == '\0' || out == NULL) {
    return false;
  }

  value = strtol(token, &end_ptr, 10);
  if (end_ptr == token || *end_ptr != '\0') {
    return false;
  }

  *out = (int)value;
  return true;
}

static bool relay_parse_csv_u32_token(const char *token, uint32_t *out) {
  unsigned long value;
  char *end_ptr;

  if (token == NULL || token[0] == '\0' || out == NULL) {
    return false;
  }

  value = strtoul(token, &end_ptr, 10);
  if (end_ptr == token || *end_ptr != '\0') {
    return false;
  }

  *out = (uint32_t)value;
  return true;
}

static bool relay_update_arduino_snapshot_from_csv(const char *line, relay_arduino_snapshot_t *snapshot) {
  enum {
    CSV_EVENT = 0,
    CSV_MS = 1,
    CSV_TEMP_C = 2,
    CSV_ADC = 3,
    CSV_DTEMP_C_PER_S = 4,
    CSV_SETPOINT_C = 5,
    CSV_MODE = 6,
    CSV_HEATER_PWM = 7,
    CSV_HEATING = 8,
    CSV_HEATER_LOCKOUT = 9,
    CSV_PUMP_ENABLED = 10,
    CSV_PUMP_ALLOWED = 11,
    CSV_PUMP_ON = 12,
    CSV_MOTOR_PWM = 13,
    CSV_MOTOR_ON_MS = 14,
    CSV_MOTOR_PERIOD_MS = 15,
    CSV_TEMP_BEFORE_PUMP_C = 16,
    CSV_MIN_TEMP_AFTER_PUMP_C = 17,
    CSV_LAST_PUMP_DROP_C = 18,
    CSV_RECOVERY_TIME_S = 19,
    CSV_MANUAL_KILL = 20,
    CSV_HARD_KILL = 21,
    CSV_UPTIME_S = 22,
    CSV_REQUIRED_FIELDS = 23,
  };

  const char *cursor = line;
  char field[32];
  bool has_more = true;
  relay_arduino_snapshot_t parsed = {0};
  int index;

  if (line == NULL || snapshot == NULL || strchr(line, ',') == NULL) {
    return false;
  }

  for (index = 0; index < CSV_REQUIRED_FIELDS; index++) {
    if (!relay_read_csv_field(&cursor, field, sizeof(field), &has_more)) {
      return false;
    }

    switch (index) {
      case CSV_EVENT:
        if (!relay_parse_csv_int_token(field, &parsed.event)) {
          return false;
        }
        break;
      case CSV_MS:
        if (!relay_parse_csv_u32_token(field, &parsed.ms)) {
          return false;
        }
        break;
      case CSV_TEMP_C:
        if (!relay_parse_csv_float_token(field, &parsed.temp_c)) {
          return false;
        }
        break;
      case CSV_ADC:
        if (!relay_parse_csv_int_token(field, &parsed.adc)) {
          return false;
        }
        break;
      case CSV_DTEMP_C_PER_S:
        if (!relay_parse_csv_float_token(field, &parsed.dtemp_c_per_s)) {
          return false;
        }
        break;
      case CSV_SETPOINT_C:
        if (!relay_parse_csv_float_token(field, &parsed.setpoint_c)) {
          return false;
        }
        break;
      case CSV_MODE:
        if (!relay_parse_csv_int_token(field, &parsed.mode)) {
          return false;
        }
        break;
      case CSV_HEATER_PWM:
        if (!relay_parse_csv_int_token(field, &parsed.heater_pwm)) {
          return false;
        }
        break;
      case CSV_HEATING:
        if (!relay_parse_csv_int_token(field, &parsed.heating)) {
          return false;
        }
        break;
      case CSV_HEATER_LOCKOUT:
        if (!relay_parse_csv_int_token(field, &parsed.heater_lockout)) {
          return false;
        }
        break;
      case CSV_PUMP_ENABLED:
        if (!relay_parse_csv_int_token(field, &parsed.pump_enabled)) {
          return false;
        }
        break;
      case CSV_PUMP_ALLOWED:
        if (!relay_parse_csv_int_token(field, &parsed.pump_allowed)) {
          return false;
        }
        break;
      case CSV_PUMP_ON:
        if (!relay_parse_csv_int_token(field, &parsed.pump_on)) {
          return false;
        }
        break;
      case CSV_MOTOR_PWM:
        if (!relay_parse_csv_int_token(field, &parsed.motor_pwm)) {
          return false;
        }
        break;
      case CSV_MOTOR_ON_MS:
        if (!relay_parse_csv_u32_token(field, &parsed.motor_on_ms)) {
          return false;
        }
        break;
      case CSV_MOTOR_PERIOD_MS:
        if (!relay_parse_csv_u32_token(field, &parsed.motor_period_ms)) {
          return false;
        }
        break;
      case CSV_TEMP_BEFORE_PUMP_C:
        if (!relay_parse_csv_float_token(field, &parsed.temp_before_pump_c)) {
          return false;
        }
        break;
      case CSV_MIN_TEMP_AFTER_PUMP_C:
        if (!relay_parse_csv_float_token(field, &parsed.min_temp_after_pump_c)) {
          return false;
        }
        break;
      case CSV_LAST_PUMP_DROP_C:
        if (!relay_parse_csv_float_token(field, &parsed.last_pump_drop_c)) {
          return false;
        }
        break;
      case CSV_RECOVERY_TIME_S:
        if (!relay_parse_csv_float_token(field, &parsed.recovery_time_s)) {
          return false;
        }
        break;
      case CSV_MANUAL_KILL:
        if (!relay_parse_csv_int_token(field, &parsed.manual_kill)) {
          return false;
        }
        break;
      case CSV_HARD_KILL:
        if (!relay_parse_csv_int_token(field, &parsed.hard_kill)) {
          return false;
        }
        break;
      case CSV_UPTIME_S:
        if (!relay_parse_csv_u32_token(field, &parsed.uptime_s)) {
          return false;
        }
        break;
      default:
        break;
    }

    if (index < CSV_REQUIRED_FIELDS - 1 && !has_more) {
      return false;
    }
  }

  parsed.have_sample = true;
  *snapshot = parsed;
  return true;
}

void relay_update_arduino_snapshot_from_line(const char *line, relay_arduino_snapshot_t *snapshot) {
  if (line == NULL || line[0] == '\0' || snapshot == NULL) {
    return;
  }

  (void)relay_update_arduino_snapshot_from_csv(line, snapshot);
}

bool relay_parse_backend_command_type(const char *type, relay_backend_command_type_t *out) {
  if (type == NULL || out == NULL) {
    return false;
  }

  if (strcmp(type, "KILL") == 0) {
    *out = RELAY_BACKEND_COMMAND_KILL;
    return true;
  }
  if (strcmp(type, "SET_ON") == 0) {
    *out = RELAY_BACKEND_COMMAND_SET_ON;
    return true;
  }
  if (strcmp(type, "OTA") == 0) {
    *out = RELAY_BACKEND_COMMAND_ESP32_OTA;
    return true;
  }
  if (strcmp(type, "ARDUINO_OTA") == 0) {
    *out = RELAY_BACKEND_COMMAND_ARDUINO_OTA;
    return true;
  }

  *out = RELAY_BACKEND_COMMAND_UNKNOWN;
  return false;
}

long relay_extract_long_json(const char *json, const char *key) {
  char pattern[48];
  const char *key_pos;
  const char *colon;
  char *end_ptr;

  if (json == NULL || key == NULL) {
    return 0;
  }

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  key_pos = strstr(json, pattern);
  if (key_pos == NULL) {
    return 0;
  }

  colon = strchr(key_pos, ':');
  if (colon == NULL) {
    return 0;
  }

  return strtol(colon + 1, &end_ptr, 10);
}

bool relay_extract_string_json(const char *json, const char *key, char *out, size_t out_len) {
  char pattern[48];
  const char *key_pos;
  const char *colon;
  const char *q1;
  const char *q2;
  size_t copy_len;

  if (json == NULL || key == NULL || out == NULL || out_len == 0) {
    return false;
  }

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  key_pos = strstr(json, pattern);
  if (key_pos == NULL) {
    out[0] = '\0';
    return false;
  }

  colon = strchr(key_pos, ':');
  if (colon == NULL) {
    out[0] = '\0';
    return false;
  }

  q1 = strchr(colon, '"');
  if (q1 == NULL) {
    out[0] = '\0';
    return false;
  }
  q2 = strchr(q1 + 1, '"');
  if (q2 == NULL) {
    out[0] = '\0';
    return false;
  }

  copy_len = (size_t)(q2 - (q1 + 1));
  if (copy_len >= out_len) {
    copy_len = out_len - 1;
  }
  memcpy(out, q1 + 1, copy_len);
  out[copy_len] = '\0';
  return true;
}
