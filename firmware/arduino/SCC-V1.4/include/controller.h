#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <Arduino.h>

enum ControlMode : uint8_t {
  CTRL_AUTOTUNE_WARMUP = 0,
  CTRL_AUTOTUNE_RELAY  = 1,
  CTRL_PID_RAMP        = 2,
  CTRL_PID_HOLD        = 3
};

extern uint32_t motorCycleStart;
extern bool motorEnabled;
extern bool heating;
extern bool heaterLockout;

extern int16_t activeSetpointCx100;
extern ControlMode controlMode;

extern float pidIntegral;
extern float pidLastTempC;
extern uint32_t pidLastMs;

extern uint32_t autotuneStartMs;

bool isMotorOnNow(uint32_t now);
bool isMotorPrebiasWindow(uint32_t now);
uint8_t applyMotorBias(uint8_t basePwm, uint32_t now);

void resetAutotune(uint32_t now);
void finishAutotune(uint32_t now, int16_t tempCx100);
uint8_t computeAutotunePwm(int16_t tempCx100, uint32_t now);
uint8_t computePidPwm(int16_t tempCx100, uint32_t now);

#endif
