#include <Arduino.h>

// ================== PINS ==================
#define THERM_PIN       A0
#define HEATER_PWM_PIN  6   // MOSFET gate for cartridge heater (PWM)

// ================== STEP SWEEP SETTINGS ==================
// Step sweep pauses at each PWM level until you type 'n' (then Enter) in Serial Monitor.
static uint8_t PWM_START = 0;
static uint8_t PWM_STEP  = 25;   // <-- sweep in bands of 50
static uint8_t PWM_MAX   = 250;  // safety cap; adjust as needed

// ADC averaging
static uint8_t ADC_AVG_N = 25;

// Print rate
static uint32_t PRINT_PERIOD_MS = 1000UL;

// ================== RUNTIME STATE ==================
enum Mode : uint8_t { IDLE = 0, STEP_SWEEP = 1 };
static Mode mode = IDLE;

static uint8_t  currentPwm = 0;
static uint32_t lastPrintMs = 0;

// ================== HELPERS ==================
static inline void heaterHardOff() {
  analogWrite(HEATER_PWM_PIN, 0);
  pinMode(HEATER_PWM_PIN, OUTPUT);
  digitalWrite(HEATER_PWM_PIN, LOW);
  currentPwm = 0;
}

static inline void heaterSet(uint8_t pwm) {
  if (pwm == 0) {
    heaterHardOff();
    return;
  }
  if (pwm > PWM_MAX) pwm = PWM_MAX;
  pinMode(HEATER_PWM_PIN, OUTPUT);
  analogWrite(HEATER_PWM_PIN, pwm);
  currentPwm = pwm;
}

uint16_t readAdcAvg(uint8_t n = 25) {
  uint32_t s = 0;
  for (uint8_t i = 0; i < n; i++) {
    s += analogRead(THERM_PIN);
    delay(3);
  }
  return (uint16_t)(s / n);
}

void printHelp() {
  Serial.println();
  Serial.println(F("Manual calibration logger (STEP sweep: waits for your input to advance)"));
  Serial.println(F("Commands:"));
  Serial.println(F("  h                 -> help"));
  Serial.println(F("  r                 -> read once (ms,pwm,adc)"));
  Serial.println(F("  p <0-255>         -> set heater PWM (0=off)"));
  Serial.println(F("  s                 -> start step-sweep (PWM_START->PWM_MAX, step=PWM_STEP)"));
  Serial.println(F("  n                 -> NEXT step (only while sweeping)"));
  Serial.println(F("  x / 0             -> stop sweep + heater off"));
  Serial.println(F("  cfg a <N>         -> set ADC averaging N"));
  Serial.println(F("  cfg start <N>     -> set PWM_START"));
  Serial.println(F("  cfg step <N>      -> set PWM_STEP"));
  Serial.println(F("  cfg max <N>       -> set PWM_MAX (cap)"));
  Serial.println(F("Output columns:"));
  Serial.println(F("  ms,pwm,adc"));
  Serial.println();
}

static inline void startStepSweep(uint32_t now) {
  mode = STEP_SWEEP;
  currentPwm = PWM_START;
  heaterSet(currentPwm);

  Serial.println(F("ms,pwm,adc"));
  Serial.println(F("# step-sweep started"));
  Serial.print(F("# holding pwm="));
  Serial.print(currentPwm);
  Serial.println(F(" (type 'n' + Enter to advance, 'x' to stop)"));
}

static inline void stopSweep() {
  mode = IDLE;
  heaterHardOff();
  Serial.println(F("# sweep stopped (heater OFF)"));
}

// Advance PWM one step, called only when user types 'n'
static inline void nextSweepStep() {
  if (mode != STEP_SWEEP) return;

  int nextPwm = (int)currentPwm + (int)PWM_STEP;
  if (nextPwm > (int)PWM_MAX) nextPwm = PWM_MAX;

  if ((uint8_t)nextPwm == currentPwm) {
    Serial.println(F("# already at PWM_MAX; stopping"));
    stopSweep();
    return;
  }

  heaterSet((uint8_t)nextPwm);
  Serial.print(F("# holding pwm="));
  Serial.println(currentPwm);
}

// Very small, robust command parser (line-based)
void handleSerial(uint32_t now) {
  static char buf[64];
  static uint8_t idx = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();

    // Immediate stop
    if (c == 'x' || c == 'X' || c == '0') {
      stopSweep();
      idx = 0;
      // flush rest of line
      while (Serial.available()) Serial.read();
      return;
    }

    if (c == '\r') continue;

    if (c == '\n') {
      buf[idx] = 0;
      idx = 0;

      // trim leading spaces
      char *p = buf;
      while (*p == ' ') p++;
      if (*p == 0) return;

      // Commands
      if (p[0] == 'h' || p[0] == 'H') { printHelp(); return; }

      if (p[0] == 'r' || p[0] == 'R') {
        uint16_t adc = readAdcAvg(ADC_AVG_N);
        Serial.print(now); Serial.print(',');
        Serial.print(currentPwm); Serial.print(',');
        Serial.println(adc);
        return;
      }

      if (p[0] == 's' || p[0] == 'S') { startStepSweep(now); return; }

      // n -> next step (only while sweeping)
      if (p[0] == 'n' || p[0] == 'N') { nextSweepStep(); return; }

      // p <value>
      if (p[0] == 'p' || p[0] == 'P') {
        int v = -1;
        if (sscanf(p + 1, "%d", &v) == 1) {
          if (v < 0) v = 0;
          if (v > 255) v = 255;
          mode = IDLE; // manual override stops sweep
          heaterSet((uint8_t)v);
          Serial.print(F("# PWM set to "));
          Serial.println(currentPwm);
        } else {
          Serial.println(F("# usage: p <0-255>"));
        }
        return;
      }

      // cfg ...
      if (strncmp(p, "cfg", 3) == 0) {
        char key[16];
        long val = 0;
        if (sscanf(p + 3, "%15s %ld", key, &val) == 2) {
          if (strcmp(key, "a") == 0) {
            if (val < 1) val = 1;
            if (val > 200) val = 200;
            ADC_AVG_N = (uint8_t)val;
            Serial.print(F("# ADC_AVG_N=")); Serial.println(ADC_AVG_N);
          } else if (strcmp(key, "start") == 0) {
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            PWM_START = (uint8_t)val;
            Serial.print(F("# PWM_START=")); Serial.println(PWM_START);
          } else if (strcmp(key, "step") == 0) {
            if (val < 1) val = 1;
            if (val > 255) val = 255;
            PWM_STEP = (uint8_t)val;
            Serial.print(F("# PWM_STEP=")); Serial.println(PWM_STEP);
          } else if (strcmp(key, "max") == 0) {
            if (val < 1) val = 1;
            if (val > 255) val = 255;
            PWM_MAX = (uint8_t)val;
            Serial.print(F("# PWM_MAX=")); Serial.println(PWM_MAX);
            if (currentPwm > PWM_MAX) heaterSet(PWM_MAX);
          } else {
            Serial.println(F("# cfg keys: a | start | step | max"));
          }
        } else {
          Serial.println(F("# usage: cfg <a|start|step|max> <value>"));
        }
        return;
      }

      Serial.println(F("# unknown command (send 'h' for help)"));
      return;
    }

    // accumulate line
    if (idx < sizeof(buf) - 1) buf[idx++] = c;
  }
}

// ================== SETUP ==================
void setup() {
  pinMode(HEATER_PWM_PIN, OUTPUT);
  heaterHardOff();

  Serial.begin(115200);
  delay(250);
  printHelp();

  Serial.println(F("# Tip: start step-sweep with 's'. Then type 'n' + Enter to advance PWM by 50."));
  Serial.println(F("# While holding each step, wait for steady state and write down your real thermometer temp."));
}

// ================== LOOP ==================
void loop() {
  uint32_t now = millis();

  handleSerial(now);

  // Periodic logging (works in IDLE and STEP_SWEEP)
  if ((now - lastPrintMs) >= PRINT_PERIOD_MS) {
    lastPrintMs = now;
    uint16_t adc = readAdcAvg(ADC_AVG_N);
    Serial.print(now);
    Serial.print(',');
    Serial.print(currentPwm);
    Serial.print(',');
    Serial.println(adc);
  }

  // Reassert LOW continuously when heater is commanded off
  if (currentPwm == 0) {
    digitalWrite(HEATER_PWM_PIN, LOW);
  }
}