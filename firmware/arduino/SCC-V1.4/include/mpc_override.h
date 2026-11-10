#ifndef MPC_OVERRIDE_H
#define MPC_OVERRIDE_H

#include <Arduino.h>
#include "config.h"

struct MpcOverrideState {
  bool active;
  uint8_t pwm;
  int16_t preheatTargetCx100;
  uint32_t lastCommandMs;
};

inline uint8_t clampMpcPwm(int value) {
  if (value <= 0) return 0;
  if (value >= HEATER_PWM_MAX) return HEATER_PWM_MAX;
  return (uint8_t)value;
}

inline int16_t clampMpcPreheatTarget(int32_t valueCx100, int16_t baseTargetCx100) {
  int16_t upperBoundCx100 = baseTargetCx100 > MPC_PREHEAT_LIMIT_Cx100
      ? baseTargetCx100
      : MPC_PREHEAT_LIMIT_Cx100;
  if (valueCx100 < baseTargetCx100) return baseTargetCx100;
  if (valueCx100 > upperBoundCx100) return upperBoundCx100;
  return (int16_t)valueCx100;
}

inline void setMpcOverride(
    MpcOverrideState &state,
    int pwm,
    int32_t preheatTargetCx100,
    int16_t baseTargetCx100,
    uint32_t now) {
  state.active = true;
  state.pwm = clampMpcPwm(pwm);
  state.preheatTargetCx100 = clampMpcPreheatTarget(preheatTargetCx100, baseTargetCx100);
  state.lastCommandMs = now;
}

inline void clearMpcOverride(MpcOverrideState &state) {
  state.active = false;
  state.pwm = 0;
  state.preheatTargetCx100 = 0;
  state.lastCommandMs = 0;
}

inline bool mpcOverrideFresh(const MpcOverrideState &state, uint32_t now) {
  return state.active && ((uint32_t)(now - state.lastCommandMs) <= MPC_COMMAND_TIMEOUT_MS);
}

inline int16_t mpcTemperatureCeilingCx100(int16_t targetCx100) {
  int32_t ceiling = (int32_t)targetCx100 + MPC_MAX_OVERSHOOT_Cx100;
  if (ceiling > OFF_LOCKOUT_HIGH_Cx100) ceiling = OFF_LOCKOUT_HIGH_Cx100;
  return (int16_t)ceiling;
}

#endif
