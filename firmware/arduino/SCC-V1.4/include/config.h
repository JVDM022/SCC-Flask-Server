#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================== PINS ==================
#define THERM_PIN        A0
#define HEATER_PWM_PIN   6    // MOSFET for heater (PWM)
#define MOTOR_PIN        9    // motor control (PWM-capable on UNO)

// ================== FINAL SETPOINT ==================
#define SETPOINT_Cx100   12500   // 125.00 C final target
#define MOTOR_PWM        155     // 0-255

// ================== CONTROL TIMING ==================
#define CONTROL_DT_MS    700
#define HEATER_PWM_MAX   200

// ================== AUTOTUNE REGION (A) ==================
// Tune in a reachable upper region with motor ON
#define AUTOTUNE_TARGET_Cx100   11500   // 115.00 C relay center
#define AUTOTUNE_WARMUP_Cx100   11000   // enter relay phase near 110.00 C
#define AUTOTUNE_BAND_Cx100       120   // +/-1.20 C around AUTOTUNE_TARGET
#define AUTOTUNE_HEAT_PWM         200
#define AUTOTUNE_CYCLES             5
#define AUTOTUNE_TIMEOUT_MS  1800000UL  // 30 minutes

// ================== PID RAMP TO FINAL ==================
#define PID_RAMP_START_Cx100   AUTOTUNE_TARGET_Cx100
#define PID_RAMP_RATE_Cx100_PER_SEC  15.0f   // 0.15 C/s ramp toward 125 C

// ================== SAFE OPERATING WINDOW ==================
// MgCl2 SCC bath must stay above 120 C and below 130 C.
#define SAFE_TEMP_LOW_Cx100       12000   // 120.00 C
#define SAFE_TEMP_HIGH_Cx100      13000   // 130.00 C

// Lock heater OFF near the upper limit, then only unlock after cooling.
#define OFF_LOCKOUT_HIGH_Cx100    12850   // 128.50 C
#define OFF_LOCKOUT_LOW_Cx100     12650   // 126.50 C

// Only allow new pump doses when the bath has enough thermal headroom.
#define PUMP_ENABLE_Cx100         12500   // allow pump at/above the 125.00 C operating setpoint
#define PUMP_DISABLE_Cx100        12300   // stop pump at/below 123.00 C
#define HEATER_BIAS_MAX_Cx100     12800   // no pump bias at/above 128.00 C

// ================== MOTOR-AWARE COMPENSATION (C) ==================
// Add heater bias shortly before pump ON and while pump is ON
#define MOTOR_PREBIAS_MS        5000UL   // start bias 5 s before motor turns on
#define MOTOR_HEAT_BIAS_PRE       18     // extra PWM before motor pulse
#define MOTOR_HEAT_BIAS_ON        28     // extra PWM during motor ON pulse

// Optional: freeze integrator during pump pulse to reduce windup from known disturbance
#define FREEZE_INTEGRAL_DURING_MOTOR_ON  1

// ================== SAFETY ==================
#define MAX_Cx100          22000
#define MIN_Cx100          -2000
#define HEATER_KILL_Cx100  SAFE_TEMP_HIGH_Cx100   // HARD KILL at 130.00 C

// NTC/ADC rail checks. Open or shorted thermistor wiring normally drives the
// ADC close to 0 or 1023; recovery thresholds add hysteresis for noisy edges.
#define ADC_SENSOR_FAULT_LOW       5
#define ADC_SENSOR_FAULT_HIGH      1018
#define ADC_SENSOR_RECOVER_LOW     10
#define ADC_SENSOR_RECOVER_HIGH    1013

// ================== MOTOR TIMING ==================
#define MOTOR_PERIOD_MS    30000UL   // cycle period
#define MOTOR_ON_MS         1000UL   // ON duration in each cycle
#define MOTOR_START_MS       250UL   // startup kick duration
#define MOTOR_START_PWM      180     // startup kick PWM

// ================== RUNTIME LIMIT ==================
#define MAX_RUNTIME_MS 345600000UL  // 96 hours

// ================== ESP32-DRIVEN UPDATE PREP ==================
// Default off: keeps UART output as CSV telemetry only unless explicitly enabled.
#ifndef ENABLE_SOFTWARE_BOOTLOADER_ENTRY
#define ENABLE_SOFTWARE_BOOTLOADER_ENTRY 0
#endif

// ================== ADC -> TEMP TRANSFER FUNCTION ==================
// Calibrated linear fit:
//   tempC = -0.1871 * adc + 167.97
#define TF_M_C_PER_ADC  (-0.1871f)
#define TF_B_C          (167.97f)

#endif
