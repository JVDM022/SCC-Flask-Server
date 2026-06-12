#include <Arduino.h>

// Standalone copy of the production Arduino motor pin/PWM setting.
// This test project does not include or modify files under firmware/.
#define MOTOR_PIN 9
#define MOTOR_PWM 155
#define STATUS_PERIOD_MS 1000UL

static bool motorEnabled = false;
static uint8_t lastPwm = 0;
static uint32_t lastStatusMs = 0;

static void motorSetPwm(uint8_t pwm) {
  analogWrite(MOTOR_PIN, pwm);
  lastPwm = pwm;
}

static void motorOff() {
  analogWrite(MOTOR_PIN, 0);
  digitalWrite(MOTOR_PIN, LOW);
  lastPwm = 0;
}

static void printStatus(const char *state, uint32_t now) {
  Serial.print(state);
  Serial.print(',');
  Serial.print(now);
  Serial.print(',');
  Serial.print(lastPwm);
  Serial.print(',');
  Serial.println(motorEnabled ? 1 : 0);
}

static void handleSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'G' || c == 'g') {
      motorEnabled = true;
      motorSetPwm(MOTOR_PWM);
      Serial.println(F("COMMAND,G,motor_on"));
    } else if (c == 'S' || c == 's') {
      motorEnabled = false;
      motorOff();
      Serial.println(F("COMMAND,S,motor_off"));
    }
  }
}

void setup() {
  pinMode(MOTOR_PIN, OUTPUT);
  motorOff();

  Serial.begin(115200);
  delay(1000);
  Serial.println(F("MOTOR_TEST_READY"));
  Serial.println(F("Send G to run motor at PWM 155. Send S to stop."));
  Serial.println(F("state,ms,pwm,enabled"));
}

void loop() {
  handleSerial();

  const uint32_t now = millis();
  if (now - lastStatusMs >= STATUS_PERIOD_MS) {
    lastStatusMs = now;
    printStatus(motorEnabled ? "RUNNING" : "STOPPED", now);
  }
}
