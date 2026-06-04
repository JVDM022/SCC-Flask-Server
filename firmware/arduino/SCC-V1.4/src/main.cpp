#include <Arduino.h>
#include "actuators.h"
#include "config.h"
#include "controller.h"
#include "pump_policy.h"
#include "telemetry_schema.h"
#include "temperature.h"

static uint32_t lastControlMs = 0;
static uint32_t programStartMs = 0;
static bool runtimeExpired = false;
static bool pumpTempAllowed = false;
static bool hardKillActive = false;

static bool pumpOnCmd = false;
static bool pumpWasOn = false;
static bool postPumpTracking = false;
static bool recoveryComplete = false;
static uint8_t pumpCmdPwm = 0;
static uint32_t lastPumpStartMs = 0;
static uint32_t lastPumpEndMs = 0;
static uint32_t recoveryTimeMs = 0;
static int16_t tempBeforeLastPumpCx100 = -32768;
static int16_t minTempAfterLastPumpCx100 = -32768;
static float tempRateCPerSec = 0.0f;
static uint32_t lastRateMs = 0;
static int16_t lastRateTempCx100 = 0;

enum CsvEvent : uint8_t {
  CSV_SAMPLE = 0,
  CSV_PUMP_START = 1,
  CSV_PUMP_END = 2,
  CSV_PUMP_RECOVERED = 3,
  CSV_HARD_KILL = 4
};

static void printCx100(int16_t valueCx100) {
  Serial.print(valueCx100 * 0.01f, 2);
}

static void printCx100OrBlank(int16_t valueCx100) {
  if (valueCx100 == -32768) {
    Serial.print(F("nan"));
    return;
  }
  printCx100(valueCx100);
}

static uint8_t controlModeCode() {
  return (uint8_t)controlMode;
}

static uint8_t computePumpPwm(uint32_t now) {
  return pumpPwmForPhase(now - motorCycleStart);
}

static void printCsvHeader() {
  Serial.println(F(TELEMETRY_CSV_HEADER));
}

static void printCsvRow(uint8_t eventCode, uint32_t now, uint16_t adc, int16_t tempCx100) {
  int16_t dropCx100 = -32768;
  if (tempBeforeLastPumpCx100 != -32768 && minTempAfterLastPumpCx100 != -32768) {
    dropCx100 = tempBeforeLastPumpCx100 - minTempAfterLastPumpCx100;
  }

  Serial.print(eventCode);
  Serial.print(',');
  Serial.print(now);
  Serial.print(',');
  printCx100(tempCx100);
  Serial.print(',');
  Serial.print(adc);
  Serial.print(',');
  Serial.print(tempRateCPerSec, 4);
  Serial.print(',');
  printCx100(activeSetpointCx100);
  Serial.print(',');
  Serial.print(controlModeCode());
  Serial.print(',');
  Serial.print(lastPwm);
  Serial.print(',');
  Serial.print(heating ? 1 : 0);
  Serial.print(',');
  Serial.print(heaterLockout ? 1 : 0);
  Serial.print(',');
  Serial.print(motorEnabled ? 1 : 0);
  Serial.print(',');
  Serial.print(pumpTempAllowed ? 1 : 0);
  Serial.print(',');
  Serial.print(pumpOnCmd ? 1 : 0);
  Serial.print(',');
  Serial.print(pumpCmdPwm);
  Serial.print(',');
  Serial.print(MOTOR_ON_MS);
  Serial.print(',');
  Serial.print(MOTOR_PERIOD_MS);
  Serial.print(',');
  printCx100OrBlank(tempBeforeLastPumpCx100);
  Serial.print(',');
  printCx100OrBlank(minTempAfterLastPumpCx100);
  Serial.print(',');
  printCx100OrBlank(dropCx100);
  Serial.print(',');
  if (recoveryComplete) {
    Serial.print(recoveryTimeMs / 1000.0f, 2);
  } else {
    Serial.print(F("nan"));
  }
  Serial.print(',');
  Serial.print(0);
  Serial.print(',');
  Serial.print(hardKillActive ? 1 : 0);
  Serial.print(',');
  Serial.println((now - programStartMs) / 1000);
}

static void updatePumpTelemetry(uint32_t now, uint16_t adc, int16_t tempCx100) {
  if (pumpOnCmd && !pumpWasOn) {
    lastPumpStartMs = now;
    tempBeforeLastPumpCx100 = tempCx100;
    minTempAfterLastPumpCx100 = tempCx100;
    recoveryTimeMs = 0;
    recoveryComplete = false;
    postPumpTracking = false;
    printCsvRow(CSV_PUMP_START, now, adc, tempCx100);
  }

  if (pumpOnCmd && tempCx100 < minTempAfterLastPumpCx100) {
    minTempAfterLastPumpCx100 = tempCx100;
  }

  if (!pumpOnCmd && pumpWasOn) {
    lastPumpEndMs = now;
    postPumpTracking = true;
    if (tempCx100 < minTempAfterLastPumpCx100) {
      minTempAfterLastPumpCx100 = tempCx100;
    }
    printCsvRow(CSV_PUMP_END, now, adc, tempCx100);
  }

  if (postPumpTracking) {
    if (tempCx100 < minTempAfterLastPumpCx100) {
      minTempAfterLastPumpCx100 = tempCx100;
    }
    if (!recoveryComplete && tempBeforeLastPumpCx100 != -32768 &&
        tempCx100 >= tempBeforeLastPumpCx100) {
      recoveryTimeMs = now - lastPumpEndMs;
      recoveryComplete = true;
      postPumpTracking = false;
      printCsvRow(CSV_PUMP_RECOVERED, now, adc, tempCx100);
    }
  }

  pumpWasOn = pumpOnCmd;
}

void setup() {
  pinMode(HEATER_PWM_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);

  heaterSet(0);
  motorSet(false);
  motorEnabled = true; // auto-enable motor timer after boot

  Serial.begin(115200);

  uint32_t now = millis();
  lastControlMs = now;
  motorCycleStart = now;
  programStartMs = now;
  runtimeExpired = false;
  pumpTempAllowed = false;
  hardKillActive = false;
  pumpOnCmd = false;
  pumpCmdPwm = 0;
  pumpWasOn = false;
  postPumpTracking = false;
  recoveryComplete = false;
  lastRateMs = 0;
  lastRateTempCx100 = 0;

  resetAutotune(now);

  printCsvHeader();
}

void loop() {
  uint32_t now = millis();

  if (!runtimeExpired && (now - programStartMs >= MAX_RUNTIME_MS)) {
    runtimeExpired = true;
    heaterSet(0);
    motorSet(false);
    pumpCmdPwm = 0;
    motorEnabled = false;
  }

  // ----- READ TEMP -----
  uint16_t adc = readAdcAvg();
  int16_t tempCx100 = adcToTempCx100(adc);

  if (lastRateMs == 0) {
    lastRateMs = now;
    lastRateTempCx100 = tempCx100;
    tempRateCPerSec = 0.0f;
  } else if (now != lastRateMs) {
    float dt = (now - lastRateMs) / 1000.0f;
    tempRateCPerSec = ((tempCx100 - lastRateTempCx100) * 0.01f) / dt;
    lastRateMs = now;
    lastRateTempCx100 = tempCx100;
  }

  if (runtimeExpired) {
    pumpOnCmd = false;
    updatePumpTelemetry(now, adc, tempCx100);
    delay(1000);
    return;
  }

  // ----- CONTROL UPDATE TIMING -----
  if ((now - lastControlMs) >= CONTROL_DT_MS) {
    lastControlMs = now;

    // Note: sensorBad is not meaningful here because adcToTempCx100() clamps.
    bool hardKill  = (tempCx100 >= HEATER_KILL_Cx100);
    bool wasHardKillActive = hardKillActive;
    hardKillActive = hardKill;
    uint8_t pwmCmd = 0;

    if (hardKill) {
      heating = false;
      heaterLockout = true;
      pidIntegral = 0.0f;
      pwmCmd = 0;
      motorSet(false);
      pumpCmdPwm = 0;
      if (!wasHardKillActive) {
        printCsvRow(CSV_HARD_KILL, now, adc, tempCx100);
      }
    } else {
      if (controlMode == CTRL_PID_RAMP || controlMode == CTRL_PID_HOLD) {
        pwmCmd = computePidPwm(tempCx100, now);
        if (tempCx100 < HEATER_BIAS_MAX_Cx100) {
          pwmCmd = applyMotorBias(pwmCmd, now);
        }
      } else {
        // During autotune, keep motor ON as required, but do not distort relay data
        // with extra feedforward bias. Warmup/relay remains clean heater command.
        pwmCmd = computeAutotunePwm(tempCx100, now);
      }
      heating = (pwmCmd > 0);
    }

    if (!heating) pwmCmd = 0;
    heaterSet(pwmCmd);
  }

  // ----- MOTOR TIMER -----
  bool pumpWasAllowed = pumpTempAllowed;
  pumpTempAllowed = nextPumpTempAllowed(pumpTempAllowed, tempCx100);

  if (!pumpWasAllowed && pumpTempAllowed) {
    motorCycleStart = now;
  }

  if (motorEnabled && pumpTempAllowed) {
    if ((now - motorCycleStart) >= MOTOR_PERIOD_MS) {
      motorCycleStart = now;
    }
    pumpOnCmd = isMotorOnNow(now);
    if (pumpOnCmd) {
      pumpCmdPwm = computePumpPwm(now);
      motorSetPwm(pumpCmdPwm);
    } else {
      pumpCmdPwm = 0;
      motorSet(false);
    }
  } else {
    pumpOnCmd = false;
    pumpCmdPwm = 0;
    motorSet(false);
  }

  updatePumpTelemetry(now, adc, tempCx100);


  if (lastPwm == 0) {
    digitalWrite(HEATER_PWM_PIN, LOW);
  }

  static uint32_t lastPrint = 0;
  if (now - lastPrint > 1000) {
    lastPrint = now;

    printCsvRow(CSV_SAMPLE, now, adc, tempCx100);
  }
}
