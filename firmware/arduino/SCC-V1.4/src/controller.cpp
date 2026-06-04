#include "controller.h"

#include "config.h"

uint32_t motorCycleStart = 0;
bool motorEnabled = false;

bool heating = false;
bool heaterLockout = false;

// Active PID setpoint ramps from tune target to final setpoint
int16_t activeSetpointCx100 = PID_RAMP_START_Cx100;
ControlMode controlMode = CTRL_AUTOTUNE_WARMUP;

// PID terms
static float kp = 6.94f;
static float ki = 0.10f;
static float kd = 120.78f;
float pidIntegral = 0.0f;
float pidLastTempC = 0.0f;
uint32_t pidLastMs = 0;

// Relay autotune bookkeeping
static bool relayHigh = true;
static int16_t relayPeakHighCx100[AUTOTUNE_CYCLES];
static int16_t relayPeakLowCx100[AUTOTUNE_CYCLES];
static uint32_t relayPeriodMs[AUTOTUNE_CYCLES];
static uint8_t relayCycleCount = 0;
static uint8_t relayPeriodCount = 0;
static uint32_t lastRiseCrossMs = 0;
static int16_t cycleMaxCx100 = -32768;
static int16_t cycleMinCx100 = 32767;
uint32_t autotuneStartMs = 0;

static float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static int16_t clampi16(int16_t x, int16_t lo, int16_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

bool isMotorOnNow(uint32_t now) {
  if (!motorEnabled) return false;
  uint32_t phase = now - motorCycleStart;
  return (phase < MOTOR_ON_MS);
}

bool isMotorPrebiasWindow(uint32_t now) {
  if (!motorEnabled) return false;
  uint32_t phase = now - motorCycleStart;
  if (phase >= MOTOR_PERIOD_MS) return false;
  return (phase >= (MOTOR_PERIOD_MS - MOTOR_PREBIAS_MS));
}

uint8_t applyMotorBias(uint8_t basePwm, uint32_t now) {
  int16_t out = basePwm;

  if (isMotorPrebiasWindow(now)) {
    out += MOTOR_HEAT_BIAS_PRE;
  }
  if (isMotorOnNow(now)) {
    out += MOTOR_HEAT_BIAS_ON;
  }

  out = clampi16(out, 0, HEATER_PWM_MAX);
  return (uint8_t)out;
}

void resetAutotune(uint32_t now) {
  controlMode = CTRL_AUTOTUNE_WARMUP;
  relayHigh = true;
  relayCycleCount = 0;
  relayPeriodCount = 0;
  lastRiseCrossMs = 0;
  cycleMaxCx100 = -32768;
  cycleMinCx100 = 32767;
  autotuneStartMs = now;

  activeSetpointCx100 = PID_RAMP_START_Cx100;
  pidIntegral = 0.0f;
  heaterLockout = false;
  heating = false;
}

void finishAutotune(uint32_t now, int16_t tempCx100) {
  bool usable = (relayCycleCount > 0 && relayPeriodCount > 0);

  if (usable) {
    float highAvgC = 0.0f;
    float lowAvgC  = 0.0f;
    float puSec    = 0.0f;

    for (uint8_t i = 0; i < relayCycleCount; i++) {
      highAvgC += relayPeakHighCx100[i] / 100.0f;
      lowAvgC  += relayPeakLowCx100[i] / 100.0f;
    }
    for (uint8_t i = 0; i < relayPeriodCount; i++) {
      puSec += relayPeriodMs[i] / 1000.0f;
    }

    highAvgC /= relayCycleCount;
    lowAvgC  /= relayCycleCount;
    puSec    /= relayPeriodCount;

    float ampC = (highAvgC - lowAvgC) * 0.5f;
    if (ampC > 0.10f && puSec > 1.0f) {
      float relayAmp = AUTOTUNE_HEAT_PWM * 0.5f;
      float ku = (4.0f * relayAmp) / (PI * ampC);

      kp = clampf(0.60f * ku, 0.10f, 80.0f);
      ki = clampf((2.0f * kp) / puSec, 0.0f, 6.0f);
      kd = clampf((kp * puSec) / 8.0f, 0.0f, 500.0f);
    } else {
      usable = false;
    }
  }

  if (!usable) {
    kp = 6.94f;
    ki = 0.10f;
    kd = 120.78f;
  }

  activeSetpointCx100 = PID_RAMP_START_Cx100;
  pidIntegral = 0.0f;
  pidLastTempC = tempCx100 / 100.0f;
  pidLastMs = now;
  controlMode = CTRL_PID_RAMP;
}

uint8_t computeAutotunePwm(int16_t tempCx100, uint32_t now) {
  if ((now - autotuneStartMs) > AUTOTUNE_TIMEOUT_MS) {
    finishAutotune(now, tempCx100);
    return 0;
  }

  if (controlMode == CTRL_AUTOTUNE_WARMUP) {
    if (tempCx100 >= AUTOTUNE_WARMUP_Cx100) {
      controlMode = CTRL_AUTOTUNE_RELAY;
      relayHigh = (tempCx100 < AUTOTUNE_TARGET_Cx100);
      cycleMaxCx100 = tempCx100;
      cycleMinCx100 = tempCx100;
      return relayHigh ? AUTOTUNE_HEAT_PWM : 0;
    }
    return AUTOTUNE_HEAT_PWM;
  }

  if (relayHigh) {
    if (tempCx100 > cycleMaxCx100) cycleMaxCx100 = tempCx100;

    if (tempCx100 >= (AUTOTUNE_TARGET_Cx100 + AUTOTUNE_BAND_Cx100)) {
      relayHigh = false;
      cycleMinCx100 = tempCx100;
      return 0;
    }
    return AUTOTUNE_HEAT_PWM;
  }

  if (tempCx100 < cycleMinCx100) cycleMinCx100 = tempCx100;

  if (tempCx100 <= (AUTOTUNE_TARGET_Cx100 - AUTOTUNE_BAND_Cx100)) {
    if (relayCycleCount < AUTOTUNE_CYCLES) {
      relayPeakHighCx100[relayCycleCount] = cycleMaxCx100;
      relayPeakLowCx100[relayCycleCount]  = cycleMinCx100;
      relayCycleCount++;
    }

    if (lastRiseCrossMs != 0 && relayPeriodCount < AUTOTUNE_CYCLES) {
      relayPeriodMs[relayPeriodCount++] = now - lastRiseCrossMs;
    }
    lastRiseCrossMs = now;

    if (relayCycleCount >= AUTOTUNE_CYCLES) {
      finishAutotune(now, tempCx100);
      return 0;
    }

    relayHigh = true;
    cycleMaxCx100 = tempCx100;
    return AUTOTUNE_HEAT_PWM;
  }

  return 0;
}

uint8_t computePidPwm(int16_t tempCx100, uint32_t now) {
  if (pidLastMs == 0) {
    pidLastMs = now;
    pidLastTempC = tempCx100 / 100.0f;
    return 0;
  }

  if (tempCx100 >= OFF_LOCKOUT_HIGH_Cx100) heaterLockout = true;
  if (tempCx100 <= OFF_LOCKOUT_LOW_Cx100)  heaterLockout = false;

  float tempC = tempCx100 / 100.0f;
  float dt = (now - pidLastMs) / 1000.0f;
  if (dt <= 0.0f) dt = CONTROL_DT_MS / 1000.0f;

  // Ramp internal setpoint from tune target to final setpoint
  if (controlMode == CTRL_PID_RAMP) {
    float step = PID_RAMP_RATE_Cx100_PER_SEC * dt;
    int16_t stepCx100 = (int16_t)(step + 0.5f);
    if (stepCx100 < 1) stepCx100 = 1;

    if (activeSetpointCx100 < SETPOINT_Cx100) {
      activeSetpointCx100 += stepCx100;
      if (activeSetpointCx100 >= SETPOINT_Cx100) {
        activeSetpointCx100 = SETPOINT_Cx100;
        controlMode = CTRL_PID_HOLD;
      }
    } else {
      activeSetpointCx100 = SETPOINT_Cx100;
      controlMode = CTRL_PID_HOLD;
    }
  } else {
    activeSetpointCx100 = SETPOINT_Cx100;
  }

  if (heaterLockout || tempCx100 >= activeSetpointCx100) {
    pidIntegral = 0.0f;
    pidLastTempC = tempC;
    pidLastMs = now;
    return 0;
  }

  float error  = (activeSetpointCx100 / 100.0f) - tempC;
  float dInput = (tempC - pidLastTempC) / dt;

  float iNext = pidIntegral;
#if FREEZE_INTEGRAL_DURING_MOTOR_ON
  if (!isMotorOnNow(now)) {
    iNext = clampf(pidIntegral + error * dt, -400.0f, 400.0f);
  }
#else
  iNext = clampf(pidIntegral + error * dt, -400.0f, 400.0f);
#endif

  float output = kp * error + ki * iNext - kd * dInput;

  if (output > HEATER_PWM_MAX) {
    output = HEATER_PWM_MAX;
    if (error > 0.0f) iNext = pidIntegral;
  } else if (output < 0.0f) {
    output = 0.0f;
    if (error < 0.0f) iNext = pidIntegral;
  }

  pidIntegral = iNext;
  pidLastTempC = tempC;
  pidLastMs = now;
  return (uint8_t)(output + 0.5f);
}
