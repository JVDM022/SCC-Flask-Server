#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <Arduino.h>

extern uint8_t lastPwm;

void motorSet(bool on);
void motorSetPwm(uint8_t pwm);
void heaterHardOff();
void heaterSet(uint8_t pwm);

#endif
