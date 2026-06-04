#include <Arduino.h>
#include <unity.h>
#include "config.h"
#include "bootloader_entry.h"
#include "pump_policy.h"
#include "telemetry_schema.h"
#include "temperature.h"

// Keep function tests explicit so the native suite does not need to build
// the Arduino sketch entrypoint in src/main.cpp.
#include "../../src/controller.cpp"
#include "../../src/bootloader_entry.cpp"
#include "../../src/temperature.cpp"

static uint8_t countCsvColumns(const char *header) {
  uint8_t columns = 1;
  while (*header) {
    if (*header == ',') {
      columns++;
    }
    header++;
  }
  return columns;
}

static const char *csvColumnAt(const char *header, uint8_t index, char *buf, size_t bufLen) {
  uint8_t current = 0;
  size_t out = 0;

  while (*header && current < index) {
    if (*header == ',') {
      current++;
    }
    header++;
  }

  while (*header && *header != ',' && out < (bufLen - 1)) {
    buf[out++] = *header++;
  }
  buf[out] = '\0';
  return buf;
}

void test_adc_to_temp_uses_calibrated_fit() {
  TEST_ASSERT_EQUAL_INT16(16797, adcToTempCx100(0));
  TEST_ASSERT_EQUAL_INT16(9313, adcToTempCx100(400));
}

void test_adc_to_temp_rounds_negative_values() {
  TEST_ASSERT_EQUAL_INT16(-5, adcToTempCx100(898));
}

void test_adc_to_temp_clamps_low_end() {
  TEST_ASSERT_EQUAL_INT16(MIN_Cx100, adcToTempCx100(1023));
}

void test_pump_temperature_gate_enables_with_headroom() {
  TEST_ASSERT_FALSE(nextPumpTempAllowed(false, PUMP_ENABLE_Cx100 - 1));
  TEST_ASSERT_TRUE(nextPumpTempAllowed(false, PUMP_ENABLE_Cx100));
  TEST_ASSERT_TRUE(nextPumpTempAllowed(false, SAFE_TEMP_HIGH_Cx100 - 1));
}

void test_pump_temperature_gate_uses_low_hysteresis() {
  TEST_ASSERT_TRUE(nextPumpTempAllowed(true, PUMP_DISABLE_Cx100 + 1));
  TEST_ASSERT_FALSE(nextPumpTempAllowed(true, PUMP_DISABLE_Cx100));
}

void test_pump_temperature_gate_blocks_upper_limit() {
  TEST_ASSERT_FALSE(nextPumpTempAllowed(false, SAFE_TEMP_HIGH_Cx100));
  TEST_ASSERT_FALSE(nextPumpTempAllowed(true, SAFE_TEMP_HIGH_Cx100));
}

void test_pump_pwm_has_startup_kick_then_normal_pwm() {
  TEST_ASSERT_TRUE(MOTOR_START_MS < MOTOR_ON_MS);
  TEST_ASSERT_EQUAL_UINT8(MOTOR_START_PWM, pumpPwmForPhase(0));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_START_PWM, pumpPwmForPhase(MOTOR_START_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_PWM, pumpPwmForPhase(MOTOR_START_MS));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_PWM, pumpPwmForPhase(MOTOR_ON_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(0, pumpPwmForPhase(MOTOR_ON_MS));
}

void test_motor_timer_uses_enabled_state_and_cycle_phase() {
  motorEnabled = false;
  motorCycleStart = 1000;
  TEST_ASSERT_FALSE(isMotorOnNow(1000));

  motorEnabled = true;
  TEST_ASSERT_TRUE(isMotorOnNow(1000));
  TEST_ASSERT_TRUE(isMotorOnNow(1000 + MOTOR_ON_MS - 1));
  TEST_ASSERT_FALSE(isMotorOnNow(1000 + MOTOR_ON_MS));
}

void test_motor_prebias_window_is_only_before_cycle_end() {
  motorEnabled = false;
  motorCycleStart = 5000;
  TEST_ASSERT_FALSE(isMotorPrebiasWindow(5000 + MOTOR_PERIOD_MS - 1));

  motorEnabled = true;
  TEST_ASSERT_FALSE(isMotorPrebiasWindow(5000));
  TEST_ASSERT_FALSE(isMotorPrebiasWindow(5000 + MOTOR_PERIOD_MS - MOTOR_PREBIAS_MS - 1));
  TEST_ASSERT_TRUE(isMotorPrebiasWindow(5000 + MOTOR_PERIOD_MS - MOTOR_PREBIAS_MS));
  TEST_ASSERT_TRUE(isMotorPrebiasWindow(5000 + MOTOR_PERIOD_MS - 1));
  TEST_ASSERT_FALSE(isMotorPrebiasWindow(5000 + MOTOR_PERIOD_MS));
}

void test_motor_bias_adds_prebias_and_on_bias_with_pwm_clamp() {
  motorEnabled = true;
  motorCycleStart = 1000;

  TEST_ASSERT_EQUAL_UINT8(10 + MOTOR_HEAT_BIAS_ON, applyMotorBias(10, 1000));
  TEST_ASSERT_EQUAL_UINT8(10 + MOTOR_HEAT_BIAS_PRE,
                          applyMotorBias(10, 1000 + MOTOR_PERIOD_MS - MOTOR_PREBIAS_MS));
  TEST_ASSERT_EQUAL_UINT8(HEATER_PWM_MAX, applyMotorBias(HEATER_PWM_MAX, 1000));
}

void test_reset_autotune_restores_initial_controller_state() {
  activeSetpointCx100 = SETPOINT_Cx100;
  controlMode = CTRL_PID_HOLD;
  pidIntegral = 12.0f;
  heaterLockout = true;
  heating = true;

  resetAutotune(12345);

  TEST_ASSERT_EQUAL_UINT8(CTRL_AUTOTUNE_WARMUP, controlMode);
  TEST_ASSERT_EQUAL_UINT32(12345, autotuneStartMs);
  TEST_ASSERT_EQUAL_INT16(PID_RAMP_START_Cx100, activeSetpointCx100);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pidIntegral);
  TEST_ASSERT_FALSE(heaterLockout);
  TEST_ASSERT_FALSE(heating);
}

void test_autotune_warmup_pwm_and_relay_transition() {
  resetAutotune(1000);

  TEST_ASSERT_EQUAL_UINT8(AUTOTUNE_HEAT_PWM, computeAutotunePwm(AUTOTUNE_WARMUP_Cx100 - 1, 1100));
  TEST_ASSERT_EQUAL_UINT8(AUTOTUNE_HEAT_PWM, computeAutotunePwm(AUTOTUNE_WARMUP_Cx100, 1200));
  TEST_ASSERT_EQUAL_UINT8(CTRL_AUTOTUNE_RELAY, controlMode);
}

void test_autotune_timeout_switches_to_pid_ramp() {
  resetAutotune(1000);

  TEST_ASSERT_EQUAL_UINT8(0, computeAutotunePwm(AUTOTUNE_WARMUP_Cx100 - 1,
                                                1000 + AUTOTUNE_TIMEOUT_MS + 1));
  TEST_ASSERT_EQUAL_UINT8(CTRL_PID_RAMP, controlMode);
  TEST_ASSERT_EQUAL_INT16(PID_RAMP_START_Cx100, activeSetpointCx100);
}

void test_pid_first_sample_initializes_without_output() {
  resetAutotune(1000);
  controlMode = CTRL_PID_RAMP;
  pidLastMs = 0;

  TEST_ASSERT_EQUAL_UINT8(0, computePidPwm(PID_RAMP_START_Cx100 - 100, 2000));
  TEST_ASSERT_EQUAL_UINT32(2000, pidLastMs);
}

void test_pid_lockout_disables_output_and_clears_integral() {
  resetAutotune(1000);
  controlMode = CTRL_PID_HOLD;
  activeSetpointCx100 = SETPOINT_Cx100;
  pidIntegral = 30.0f;
  pidLastMs = 2000;
  pidLastTempC = OFF_LOCKOUT_HIGH_Cx100 / 100.0f;

  TEST_ASSERT_EQUAL_UINT8(0, computePidPwm(OFF_LOCKOUT_HIGH_Cx100, 3000));
  TEST_ASSERT_TRUE(heaterLockout);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pidIntegral);
}

void test_pid_ramp_advances_to_final_hold() {
  resetAutotune(1000);
  controlMode = CTRL_PID_RAMP;
  activeSetpointCx100 = SETPOINT_Cx100 - 1;
  pidLastMs = 2000;
  pidLastTempC = (SETPOINT_Cx100 - 200) / 100.0f;

  (void)computePidPwm(SETPOINT_Cx100 - 200, 3000);

  TEST_ASSERT_EQUAL_INT16(SETPOINT_Cx100, activeSetpointCx100);
  TEST_ASSERT_EQUAL_UINT8(CTRL_PID_HOLD, controlMode);
}

void test_telemetry_header_column_count_matches_schema_constant() {
  TEST_ASSERT_EQUAL_UINT8(TELEMETRY_CSV_COLUMNS, countCsvColumns(TELEMETRY_CSV_HEADER));
}

void test_telemetry_header_uses_expected_column_order() {
  const char *expected[] = {
    "event",
    "ms",
    "temp_c",
    "adc",
    "dtemp_c_per_s",
    "setpoint_c",
    "mode",
    "heater_pwm",
    "heating",
    "heater_lockout",
    "pump_enabled",
    "pump_allowed",
    "pump_on",
    "motor_pwm",
    "motor_on_ms",
    "motor_period_ms",
    "temp_before_pump_c",
    "min_temp_after_pump_c",
    "last_pump_drop_c",
    "recovery_time_s",
    "manual_kill",
    "hard_kill",
    "uptime_s",
  };
  char column[32];

  for (uint8_t i = 0; i < TELEMETRY_CSV_COLUMNS; i++) {
    TEST_ASSERT_EQUAL_STRING(expected[i], csvColumnAt(TELEMETRY_CSV_HEADER, i, column, sizeof(column)));
  }
}

void test_bootloader_entry_accepts_supported_commands() {
  TEST_ASSERT_TRUE(isBootloaderEntryCommand("ENTER_BOOTLOADER"));
  TEST_ASSERT_TRUE(isBootloaderEntryCommand("OTA_PREPARE"));
  TEST_ASSERT_TRUE(isBootloaderEntryCommand(" enter_bootloader "));
  TEST_ASSERT_TRUE(isBootloaderEntryCommand("\tota_prepare\t"));
}

void test_bootloader_entry_rejects_other_lines() {
  TEST_ASSERT_FALSE(isBootloaderEntryCommand(NULL));
  TEST_ASSERT_FALSE(isBootloaderEntryCommand(""));
  TEST_ASSERT_FALSE(isBootloaderEntryCommand("STATUS"));
  TEST_ASSERT_FALSE(isBootloaderEntryCommand("OTA_PREPARE_NOW"));
  TEST_ASSERT_FALSE(isBootloaderEntryCommand("ENTER_BOOTLOADER EXTRA"));
}

static void runFunctionTests() {
  RUN_TEST(test_adc_to_temp_uses_calibrated_fit);
  RUN_TEST(test_adc_to_temp_rounds_negative_values);
  RUN_TEST(test_adc_to_temp_clamps_low_end);
  RUN_TEST(test_pump_temperature_gate_enables_with_headroom);
  RUN_TEST(test_pump_temperature_gate_uses_low_hysteresis);
  RUN_TEST(test_pump_temperature_gate_blocks_upper_limit);
  RUN_TEST(test_pump_pwm_has_startup_kick_then_normal_pwm);
  RUN_TEST(test_motor_timer_uses_enabled_state_and_cycle_phase);
  RUN_TEST(test_motor_prebias_window_is_only_before_cycle_end);
  RUN_TEST(test_motor_bias_adds_prebias_and_on_bias_with_pwm_clamp);
  RUN_TEST(test_reset_autotune_restores_initial_controller_state);
  RUN_TEST(test_autotune_warmup_pwm_and_relay_transition);
  RUN_TEST(test_autotune_timeout_switches_to_pid_ramp);
  RUN_TEST(test_pid_first_sample_initializes_without_output);
  RUN_TEST(test_pid_lockout_disables_output_and_clears_integral);
  RUN_TEST(test_pid_ramp_advances_to_final_hold);
  RUN_TEST(test_telemetry_header_column_count_matches_schema_constant);
  RUN_TEST(test_telemetry_header_uses_expected_column_order);
  RUN_TEST(test_bootloader_entry_accepts_supported_commands);
  RUN_TEST(test_bootloader_entry_rejects_other_lines);
}

#ifdef ARDUINO
void setup() {
  delay(2000);
  UNITY_BEGIN();
  runFunctionTests();
  UNITY_END();
}

void loop() {
}
#else
int main() {
  UNITY_BEGIN();
  runFunctionTests();
  return UNITY_END();
}
#endif
