#ifndef PUMP_POLICY_H
#define PUMP_POLICY_H

#include "config.h"

static inline bool nextPumpTempAllowedAt(
    bool currentAllowed,
    int16_t tempCx100,
    int16_t enableTempCx100) {
  bool allowed = currentAllowed;

  if (tempCx100 >= enableTempCx100 && tempCx100 < SAFE_TEMP_HIGH_Cx100) {
    allowed = true;
  }
  if (tempCx100 <= PUMP_DISABLE_Cx100 || tempCx100 >= SAFE_TEMP_HIGH_Cx100) {
    allowed = false;
  }

  return allowed;
}

static inline bool nextPumpTempAllowed(bool currentAllowed, int16_t tempCx100) {
  return nextPumpTempAllowedAt(currentAllowed, tempCx100, PUMP_ENABLE_Cx100);
}

static inline uint8_t pumpPwmForPhase(uint32_t phaseMs) {
  if (phaseMs >= MOTOR_ON_MS) {
    return 0;
  }
  if (phaseMs < MOTOR_START_MS) {
    return MOTOR_START_PWM;
  }
  return MOTOR_PWM;
}

#endif
