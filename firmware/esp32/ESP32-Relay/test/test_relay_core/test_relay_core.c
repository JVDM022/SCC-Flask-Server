#include <string.h>

#include "relay_core.h"
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

static void test_copy_helpers_are_bounded(void) {
  char dest[6];

  relay_copy_cstr(dest, sizeof(dest), "abcdef");
  TEST_ASSERT_EQUAL_STRING("abcde", dest);

  relay_copy_cstr(dest, sizeof(dest), NULL);
  TEST_ASSERT_EQUAL_STRING("", dest);
}

static void test_trim_inplace_removes_outer_whitespace(void) {
  char value[] = " \t KILL 1 \r\n";

  relay_trim_inplace(value);

  TEST_ASSERT_EQUAL_STRING("KILL 1", value);
}

static void test_update_arduino_snapshot_from_csv_row(void) {
  relay_arduino_snapshot_t snapshot = {0};

  relay_update_arduino_snapshot_from_line(
      "0,123456,126.42,222,-0.1300,125.00,3,184,1,0,1,1,0,155,150,30000,127.20,124.80,2.40,18.50,0,0,300",
      &snapshot);

  TEST_ASSERT_TRUE(snapshot.have_sample);
  TEST_ASSERT_EQUAL_INT(0, snapshot.event);
  TEST_ASSERT_EQUAL_UINT32(123456, snapshot.ms);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 126.42f, snapshot.temp_c);
  TEST_ASSERT_EQUAL_INT(222, snapshot.adc);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.1300f, snapshot.dtemp_c_per_s);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 125.00f, snapshot.setpoint_c);
  TEST_ASSERT_EQUAL_INT(3, snapshot.mode);
  TEST_ASSERT_EQUAL_INT(184, snapshot.heater_pwm);
  TEST_ASSERT_EQUAL_INT(1, snapshot.heating);
  TEST_ASSERT_EQUAL_INT(0, snapshot.heater_lockout);
  TEST_ASSERT_EQUAL_INT(1, snapshot.pump_enabled);
  TEST_ASSERT_EQUAL_INT(1, snapshot.pump_allowed);
  TEST_ASSERT_EQUAL_INT(0, snapshot.pump_on);
  TEST_ASSERT_EQUAL_INT(155, snapshot.motor_pwm);
  TEST_ASSERT_EQUAL_UINT32(150, snapshot.motor_on_ms);
  TEST_ASSERT_EQUAL_UINT32(30000, snapshot.motor_period_ms);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 127.20f, snapshot.temp_before_pump_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 124.80f, snapshot.min_temp_after_pump_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.40f, snapshot.last_pump_drop_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 18.50f, snapshot.recovery_time_s);
  TEST_ASSERT_EQUAL_INT(0, snapshot.manual_kill);
  TEST_ASSERT_EQUAL_INT(0, snapshot.hard_kill);
  TEST_ASSERT_EQUAL_UINT32(300, snapshot.uptime_s);
}

static void test_update_arduino_snapshot_keeps_individual_kill_flags(void) {
  relay_arduino_snapshot_t snapshot = {0};

  relay_update_arduino_snapshot_from_line(
      "0,123456,126.42,222,-0.1300,125.00,3,184,0,0,1,1,1,155,150,30000,127.20,124.80,2.40,18.50,1,0,300",
      &snapshot);

  TEST_ASSERT_TRUE(snapshot.have_sample);
  TEST_ASSERT_EQUAL_INT(0, snapshot.heating);
  TEST_ASSERT_EQUAL_INT(1, snapshot.pump_on);
  TEST_ASSERT_EQUAL_INT(1, snapshot.manual_kill);
  TEST_ASSERT_EQUAL_INT(0, snapshot.hard_kill);
}

static void test_update_arduino_snapshot_ignores_non_csv_status_lines(void) {
  relay_arduino_snapshot_t snapshot = {
      .have_sample = true,
      .temp_c = 20.0f,
      .heating = 1,
  };

  relay_update_arduino_snapshot_from_line("not a csv status row", &snapshot);

  TEST_ASSERT_TRUE(snapshot.have_sample);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, snapshot.temp_c);
  TEST_ASSERT_EQUAL_INT(1, snapshot.heating);
  TEST_ASSERT_EQUAL_INT(0, snapshot.pump_on);
}

static void test_extract_json_values(void) {
  char type[8];
  char url[64];

  TEST_ASSERT_EQUAL_INT(42, relay_extract_long_json("{\"cmdId\":42,\"value\":1,\"type\":\"KILL\"}", "cmdId"));
  TEST_ASSERT_TRUE(relay_extract_string_json("{\"cmdId\":42,\"type\":\"KILL\"}", "type", type, sizeof(type)));
  TEST_ASSERT_EQUAL_STRING("KILL", type);

  TEST_ASSERT_TRUE(relay_extract_string_json(
      "{\"type\":\"ARDUINO_OTA\",\"url\":\"https://example.test/uno.hex\"}",
      "url",
      url,
      sizeof(url)));
  TEST_ASSERT_EQUAL_STRING("https://example.test/uno.hex", url);
}

static void test_parse_backend_command_type_accepts_supported_commands(void) {
  relay_backend_command_type_t command_type = RELAY_BACKEND_COMMAND_UNKNOWN;

  TEST_ASSERT_TRUE(relay_parse_backend_command_type("KILL", &command_type));
  TEST_ASSERT_EQUAL_INT(RELAY_BACKEND_COMMAND_KILL, command_type);

  TEST_ASSERT_TRUE(relay_parse_backend_command_type("SET_ON", &command_type));
  TEST_ASSERT_EQUAL_INT(RELAY_BACKEND_COMMAND_SET_ON, command_type);

  TEST_ASSERT_TRUE(relay_parse_backend_command_type("OTA", &command_type));
  TEST_ASSERT_EQUAL_INT(RELAY_BACKEND_COMMAND_ESP32_OTA, command_type);

  TEST_ASSERT_TRUE(relay_parse_backend_command_type("ARDUINO_OTA", &command_type));
  TEST_ASSERT_EQUAL_INT(RELAY_BACKEND_COMMAND_ARDUINO_OTA, command_type);

  TEST_ASSERT_FALSE(relay_parse_backend_command_type("REBOOT", &command_type));
  TEST_ASSERT_EQUAL_INT(RELAY_BACKEND_COMMAND_UNKNOWN, command_type);
}

static int run_relay_core_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_copy_helpers_are_bounded);
  RUN_TEST(test_trim_inplace_removes_outer_whitespace);
  RUN_TEST(test_update_arduino_snapshot_from_csv_row);
  RUN_TEST(test_update_arduino_snapshot_keeps_individual_kill_flags);
  RUN_TEST(test_update_arduino_snapshot_ignores_non_csv_status_lines);
  RUN_TEST(test_extract_json_values);
  RUN_TEST(test_parse_backend_command_type_accepts_supported_commands);
  return UNITY_END();
}

#ifdef ESP_PLATFORM
void app_main(void) {
  (void)run_relay_core_tests();
}
#else
int main(void) {
  return run_relay_core_tests();
}
#endif
