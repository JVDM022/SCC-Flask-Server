#include <Arduino.h>

// Hardware and calibration values copied from the production Arduino firmware
// for standalone testing only. Do not include production firmware files here.
#define THERM_PIN A0
#define HEATER_PWM_PIN 6

#define TARGET_TEMP_C 40.0f
#define HEATER_PWM_MAX 200
#define HEATER_KILL_C 50.0f
#define HEATER_UNLOCK_C 45.5f

#define TF_M_C_PER_ADC (-0.1871f)
#define TF_B_C 167.97f

#define CONTROL_DT_MS 700UL
#define STATUS_DT_MS 1000UL

static bool heaterEnabled = false;
static bool hardKill = false;
static uint8_t lastPwm = 0;
static uint32_t lastControlMs = 0;
static uint32_t lastStatusMs = 0;

static uint16_t readAdcAvg(uint8_t samples = 8) {
  uint32_t total = 0;
  for (uint8_t i = 0; i < samples; i++) {
    total += analogRead(THERM_PIN);
    delay(2);
  }
  return static_cast<uint16_t>(total / samples);
}

static float adcToTempC(uint16_t adc) {
  return (TF_M_C_PER_ADC * static_cast<float>(adc)) + TF_B_C;
}

static void heaterSet(uint8_t pwm) {
  if (pwm > HEATER_PWM_MAX) {
    pwm = HEATER_PWM_MAX;
  }
  analogWrite(HEATER_PWM_PIN, pwm);
  lastPwm = pwm;
}

static void heaterOff() {
  analogWrite(HEATER_PWM_PIN, 0);
  digitalWrite(HEATER_PWM_PIN, LOW);
  lastPwm = 0;
}

static uint8_t computeHoldPwm(float tempC) {
  const float errorC = TARGET_TEMP_C - tempC;

  if (errorC <= -0.30f) {
    return 0;
  }
  if (errorC < 0.20f) {
    return 45;
  }
  if (errorC < 1.00f) {
    return 80;
  }
  if (errorC < 3.00f) {
    return 130;
  }
  return HEATER_PWM_MAX;
}

static void printStatus(const char *state, uint32_t now, uint16_t adc, float tempC) {
  Serial.print(state);
  Serial.print(',');
  Serial.print(now);
  Serial.print(',');
  Serial.print(tempC, 2);
  Serial.print(',');
  Serial.print(adc);
  Serial.print(',');
  Serial.print(TARGET_TEMP_C, 2);
  Serial.print(',');
  Serial.print(lastPwm);
  Serial.print(',');
  Serial.print(heaterEnabled ? 1 : 0);
  Serial.print(',');
  Serial.println(hardKill ? 1 : 0);
}

static void handleSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'G' || c == 'g') {
      if (!hardKill) {
        heaterEnabled = true;
        Serial.println(F("COMMAND,G,heater_enabled"));
      } else {
        Serial.println(F("COMMAND,G,rejected_hard_kill"));
      }
    } else if (c == 'S' || c == 's') {
      heaterEnabled = false;
      heaterOff();
      Serial.println(F("COMMAND,S,heater_stopped"));
    }
  }
}

void setup() {
  pinMode(HEATER_PWM_PIN, OUTPUT);
  heaterOff();

  Serial.begin(115200);
  delay(1000);
  Serial.println(F("HEATER_TEST_READY"));
  Serial.println(F("Send G to hold 120.00 C. Send S to stop."));
  Serial.println(F("state,ms,temp_c,adc,setpoint_c,pwm,enabled,hard_kill"));
}

void loop() {
  handleSerial();

  const uint32_t now = millis();
  const uint16_t adc = readAdcAvg();
  const float tempC = adcToTempC(adc);

  if (tempC >= HEATER_KILL_C) {
    hardKill = true;
    heaterEnabled = false;
    heaterOff();
  } else if (hardKill && tempC <= HEATER_UNLOCK_C) {
    hardKill = false;
  }

  if (now - lastControlMs >= CONTROL_DT_MS) {
    lastControlMs = now;
    if (heaterEnabled && !hardKill) {
      heaterSet(computeHoldPwm(tempC));
    } else {
      heaterOff();
    }
  }

  if (now - lastStatusMs >= STATUS_DT_MS) {
    lastStatusMs = now;
    if (hardKill) {
      printStatus("HARD_KILL", now, adc, tempC);
    } else if (heaterEnabled) {
      printStatus("HOLDING", now, adc, tempC);
    } else {
      printStatus("STOPPED", now, adc, tempC);
    }
  }
}
