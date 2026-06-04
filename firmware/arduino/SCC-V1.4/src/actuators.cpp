#include "actuators.h"

#include "config.h"

uint8_t lastPwm = 0;

void motorSet(bool on) {
  analogWrite(MOTOR_PIN, on ? MOTOR_PWM : 0);
}

void motorSetPwm(uint8_t pwm) {
  analogWrite(MOTOR_PIN, pwm);
}

void heaterHardOff() {
  analogWrite(HEATER_PWM_PIN, 0);
  pinMode(HEATER_PWM_PIN, OUTPUT);
  digitalWrite(HEATER_PWM_PIN, LOW);
  lastPwm = 0;
}

void heaterSet(uint8_t pwm) {
  if (pwm == 0) {
    heaterHardOff();
    return;
  }
  if (pwm > HEATER_PWM_MAX) pwm = HEATER_PWM_MAX;
  pinMode(HEATER_PWM_PIN, OUTPUT);
  analogWrite(HEATER_PWM_PIN, pwm);
  lastPwm = pwm;
}
