#include <Arduino.h>

// ================== PINS ==================
#define THERM_PIN        A0
#define HEATER_PWM_PIN   6    // MOSFET for heater (PWM)
#define MOTOR_PIN        9    // motor control (PWM-capable on UNO)

// ================== SETPOINT ==================
#define SETPOINT_Cx100   10000   // 100.00 C
#define MOTOR_PWM        155     // 0–255

// ================== PID + AUTOTUNE ==================
#define CONTROL_DT_MS    700
#define HEATER_PWM_MAX   200

// Lock heater OFF above setpoint, then only unlock after cooling below this band
#define OFF_LOCKOUT_HIGH_Cx100 10020  // 100.20C
#define OFF_LOCKOUT_LOW_Cx100   9950  // 99.50C

// Relay autotune around setpoint
#define AUTOTUNE_WARMUP_Cx100   9700   // start relay phase near 97.00C (below setpoint)
#define AUTOTUNE_BAND_Cx100      150   // +/-1.50C band
#define AUTOTUNE_HEAT_PWM        200
#define AUTOTUNE_CYCLES            5
#define AUTOTUNE_TIMEOUT_MS  1800000UL // 30 minutes

// ================== SAFETY ==================
#define MAX_Cx100          22000
#define MIN_Cx100          -2000
#define HEATER_KILL_Cx100  16000   // HARD KILL at 160.00C

// ================== MOTOR TIMING ==================
#define MOTOR_PERIOD_MS   180000UL   // cycle period
#define MOTOR_ON_MS         2000UL   // ON duration in each cycle

// ================== RUNTIME LIMIT ==================
#define MAX_RUNTIME_MS 345600000UL  // 96 hours

// ================== ADC -> TEMP TRANSFER FUNCTION ==================
// Calibrated linear fit:
//   tempC = -0.1871 * adc + 167.97
#define TF_M_C_PER_ADC  (-0.1871f)   // slope (°C per ADC count)
#define TF_B_C          (167.97f)    // intercept (°C)

// ================== GLOBALS ==================
uint32_t lastControlMs = 0;

uint32_t motorCycleStart = 0;
bool motorEnabled = false;     // Serial 1/0 controls this

uint32_t programStartMs = 0;
bool runtimeExpired = false;
bool manualKill = false;       // Latched manual kill via Serial ('K' to kill, 'U' to clear)

uint8_t lastPwm = 0;

static bool heating = false;
static bool heaterLockout = false;
static bool pidAutoTuned = false;

enum ControlMode : uint8_t {
  CTRL_AUTOTUNE_WARMUP = 0,
  CTRL_AUTOTUNE_RELAY  = 1,
  CTRL_PID             = 2
};
ControlMode controlMode = CTRL_AUTOTUNE_WARMUP;

// PID terms
float kp = 6.94f;
float ki = 0.10f;
float kd = 120.78f;
float pidIntegral = 0.0f;
float pidLastTempC = 0.0f;
uint32_t pidLastMs = 0;

// Relay autotune bookkeeping
bool relayHigh = true;
int16_t relayPeakHighCx100[AUTOTUNE_CYCLES];
int16_t relayPeakLowCx100[AUTOTUNE_CYCLES];
uint32_t relayPeriodMs[AUTOTUNE_CYCLES];
uint8_t relayCycleCount = 0;
uint8_t relayPeriodCount = 0;
uint32_t lastRiseCrossMs = 0;
int16_t cycleMaxCx100 = -32768;
int16_t cycleMinCx100 = 32767;
uint32_t autotuneStartMs = 0;

// ================== HELPERS ==================
uint16_t readAdcAvg(uint8_t n = 8) {
  uint32_t s = 0;
  for (uint8_t i = 0; i < n; i++) s += analogRead(THERM_PIN);
  return (uint16_t)(s / n);
}

int16_t adcToTempCx100(uint16_t adc) {
  float tempC = (TF_M_C_PER_ADC * (float)adc) + TF_B_C;

  if (tempC > (MAX_Cx100 / 100.0f)) tempC = (MAX_Cx100 / 100.0f);
  if (tempC < (MIN_Cx100 / 100.0f)) tempC = (MIN_Cx100 / 100.0f);

  return (int16_t)(tempC * 100.0f + (tempC >= 0 ? 0.5f : -0.5f));
}

static inline void motorSet(bool on) {
  analogWrite(MOTOR_PIN, on ? MOTOR_PWM : 0);
}

static inline float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static inline void heaterHardOff() {
  analogWrite(HEATER_PWM_PIN, 0);
  pinMode(HEATER_PWM_PIN, OUTPUT);
  digitalWrite(HEATER_PWM_PIN, LOW);
  lastPwm = 0;
}

static inline void heaterSet(uint8_t pwm) {
  if (pwm == 0) {
    heaterHardOff();
    return;
  }
  if (pwm > HEATER_PWM_MAX) pwm = HEATER_PWM_MAX;
  pinMode(HEATER_PWM_PIN, OUTPUT);
  analogWrite(HEATER_PWM_PIN, pwm);
  lastPwm = pwm;
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

  pidIntegral = 0.0f;
  heaterLockout = false;
  pidAutoTuned = false;
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
      lowAvgC  += relayPeakLowCx100[i]  / 100.0f;
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

      pidAutoTuned = true;
    } else {
      usable = false;
    }
  }

  if (!usable) {
    kp = 6.94f;
    ki = 0.10f;
    kd = 120.78f;
    pidAutoTuned = false;
  }

  pidIntegral = 0.0f;
  pidLastTempC = tempCx100 / 100.0f;
  pidLastMs = now;
  controlMode = CTRL_PID;

  Serial.print("PID ready. Kp=");
  Serial.print(kp, 4);
  Serial.print(" Ki=");
  Serial.print(ki, 4);
  Serial.print(" Kd=");
  Serial.print(kd, 4);
  Serial.println(pidAutoTuned ? " (autotuned)" : " (fallback)");
}

uint8_t computeAutotunePwm(int16_t tempCx100, uint32_t now) {
  if ((now - autotuneStartMs) > AUTOTUNE_TIMEOUT_MS) {
    Serial.println("Autotune timeout. Switching to fallback PID gains.");
    finishAutotune(now, tempCx100);
    return 0;
  }

  if (controlMode == CTRL_AUTOTUNE_WARMUP) {
    // Warmup until near the relay-start temperature, then begin relay tuning.
    if (tempCx100 >= AUTOTUNE_WARMUP_Cx100) {
      controlMode = CTRL_AUTOTUNE_RELAY;
      relayHigh = (tempCx100 < SETPOINT_Cx100);
      cycleMaxCx100 = tempCx100;
      cycleMinCx100 = tempCx100;
      Serial.println("Autotune relay phase started.");
      return relayHigh ? AUTOTUNE_HEAT_PWM : 0;
    }

    // Full power warmup
    return AUTOTUNE_HEAT_PWM;
  }

  // Relay (bang-bang) autotune around setpoint +/- band
  if (relayHigh) {
    if (tempCx100 > cycleMaxCx100) cycleMaxCx100 = tempCx100;

    if (tempCx100 >= (SETPOINT_Cx100 + AUTOTUNE_BAND_Cx100)) {
      relayHigh = false;
      cycleMinCx100 = tempCx100;
      return 0;
    }
    return AUTOTUNE_HEAT_PWM;
  }

  if (tempCx100 < cycleMinCx100) cycleMinCx100 = tempCx100;

  if (tempCx100 <= (SETPOINT_Cx100 - AUTOTUNE_BAND_Cx100)) {
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

  // Hysteresis lockout
  if (tempCx100 >= OFF_LOCKOUT_HIGH_Cx100) heaterLockout = true;
  if (tempCx100 <= OFF_LOCKOUT_LOW_Cx100)  heaterLockout = false;

  float tempC = tempCx100 / 100.0f;
  float dt = (now - pidLastMs) / 1000.0f;
  if (dt <= 0.0f) dt = CONTROL_DT_MS / 1000.0f;

  // Hard-ish behavior: if at/above setpoint, force heater off and clear integral
  if (heaterLockout || tempCx100 >= SETPOINT_Cx100) {
    pidIntegral = 0.0f;
    pidLastTempC = tempC;
    pidLastMs = now;
    return 0;
  }

  float error  = (SETPOINT_Cx100 / 100.0f) - tempC;
  float dInput = (tempC - pidLastTempC) / dt;

  float iNext  = clampf(pidIntegral + error * dt, -400.0f, 400.0f);
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

// ================== SETUP ==================
void setup() {
  pinMode(HEATER_PWM_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);

  heaterSet(0);
  motorSet(false);
  motorEnabled = true; // auto-enable motor timer after every reset/boot

  Serial.begin(115200);

  uint32_t now = millis();
  lastControlMs = now;
  motorCycleStart = now;
  programStartMs = now;
  runtimeExpired = false;

  // ALWAYS autotune every reset, then PID holds 100C.
  resetAutotune(now);

  Serial.println("Boot: AUTOTUNE every reset -> then PID hold at 100C.");
  Serial.println("Motor timer auto-enabled on reset. Send '0' to disable, '1' to re-enable.");
  Serial.println("Send 't' to restart PID autotune.");
  Serial.println("Send 'K' to MANUAL KILL (heater+motor OFF, latched). Send 'U' to clear.");
  Serial.println("Safety: HARD-KILL at 160C, forced heater LOW when off, runtime limit 96h.");
}

// ================== LOOP ==================
void loop() {
  uint32_t now = millis();

  // ===== HARD STOP (heater + motor) =====
  if (!runtimeExpired && (now - programStartMs >= MAX_RUNTIME_MS)) {
    runtimeExpired = true;
    heaterSet(0);
    motorSet(false);
    motorEnabled = false;
    Serial.println("⛔ Runtime limit reached. System shut down.");
  }

  if (runtimeExpired) {
    delay(1000);
    return;
  }

  // ----- READ TEMP -----
  uint16_t adc = readAdcAvg();
  int16_t tempCx100 = adcToTempCx100(adc);
  float tempC_now = tempCx100 * 0.01f;

  // ----- SERIAL CONTROL -----
  // Commands:
  //   K = manual kill (latched): heater OFF + motor OFF + motor timer disabled
  //   U = clear manual kill
  //   0 = disable motor timer
  //   1 = enable motor timer
  //   t/T = restart PID autotune
  while (Serial.available()) {
    char c = Serial.read();

    if (c == 'K' || c == 'k') {
      manualKill = true;
      heaterSet(0);
      motorEnabled = false;
      motorSet(false);
      Serial.println("🛑 MANUAL KILL: heater OFF, motor OFF (latched)");
    } else if (c == 'U' || c == 'u') {
      manualKill = false;
      Serial.println("✅ MANUAL KILL CLEARED");
      pidIntegral = 0.0f;
      pidLastTempC = tempC_now;
      pidLastMs = now;
    } else if (c == '0') {
      motorEnabled = false;
      motorSet(false);
      Serial.println("Motor DISABLED (stopped)");
    } else if (c == '1') {
      motorEnabled = true;
      motorCycleStart = now;
      Serial.println("Motor ENABLED (timer running)");
    } else if (c == 't' || c == 'T') {
      Serial.println("Restarting autotune...");
      resetAutotune(now);
    }
  }

  // ----- MANUAL KILL OVERRIDE -----
  if (manualKill) {
    heaterSet(0);
    motorSet(false);
    delay(50);
    return;
  }

  // ----- CONTROL UPDATE TIMING -----
  if ((now - lastControlMs) >= CONTROL_DT_MS) {
    lastControlMs = now;

    // ----- SAFETY CHECKS -----
    bool sensorBad = (tempCx100 < MIN_Cx100 || tempCx100 > MAX_Cx100);
    bool hardKill  = (tempCx100 >= HEATER_KILL_Cx100);
    uint8_t pwmCmd = 0;

    if (sensorBad || hardKill) {
      heating = false;
      heaterLockout = true;
      pidIntegral = 0.0f;
      pwmCmd = 0;
    } else {
      if (controlMode == CTRL_PID) {
        pwmCmd = computePidPwm(tempCx100, now);
      } else {
        pwmCmd = computeAutotunePwm(tempCx100, now);
      }
      heating = (pwmCmd > 0);
    }

    if (!heating) pwmCmd = 0;
    heaterSet(pwmCmd);
  }

  // ----- MOTOR TIMER (only if enabled) -----
  if (motorEnabled) {
    if ((now - motorCycleStart) >= MOTOR_PERIOD_MS) {
      motorCycleStart = now;
    }
    motorSet(((now - motorCycleStart) < MOTOR_ON_MS));
  } else {
    motorSet(false);
  }

  // Reassert LOW continuously when heater is commanded off.
  if (lastPwm == 0) {
    digitalWrite(HEATER_PWM_PIN, LOW);
  }

  // ----- DEBUG -----
  // ----- UART STATUS OUTPUT -----
static uint32_t lastPrint = 0;

if (now - lastPrint > 1000) {
  lastPrint = now;

  uint32_t uptimeSec = (now - programStartMs) / 1000;
  uint32_t uptimeMin = uptimeSec / 60;
  uint32_t uptimeRemSec = uptimeSec % 60;

  Serial.print("T=");
Serial.print(tempCx100 * 0.01f, 2);
Serial.print(",ON=");
Serial.print(heating ? 1 : 0);
Serial.print(",KILL=");
Serial.println(manualKill ? 1 : 0);

Serial.print("HEAT=");
Serial.print(heating ? "ON" : "OFF");
Serial.print(",ADC=");
Serial.print(adc);
Serial.print(",TEMP=");
Serial.print(tempCx100 * 0.01f, 2);
Serial.print(",PWM=");
Serial.print(lastPwm);
Serial.print(",MOTOR=");
Serial.print((motorEnabled && ((now - motorCycleStart) < MOTOR_ON_MS)) ? "ON" : "OFF");
Serial.print(",UPTIME=");
Serial.print(uptimeMin);
Serial.print("m");
Serial.print(uptimeRemSec);
Serial.println("s");

}
}