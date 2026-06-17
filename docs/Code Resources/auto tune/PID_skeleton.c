#include <Arduino.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

struct RelayAutotune {
  int16_t setpointCx100 = 10000;
  int16_t bandCx100 = 150;
  uint8_t heatPwm = 200;
  uint8_t maxCycles = 5;
  uint32_t timeoutMs = 30UL * 60UL * 1000UL;

  bool active = false;
  bool relayHigh = true;
  uint8_t cyclesDone = 0;
  uint8_t periodsDone = 0;

  int16_t cycleMaxCx100 = INT16_MIN;
  int16_t cycleMinCx100 = INT16_MAX;

  uint32_t startMs = 0;
  uint32_t lastLowCrossMs = 0;

  float sumHighC = 0.0f;
  float sumLowC = 0.0f;
  float sumPeriodSec = 0.0f;
};

static inline float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

void autotuneStart(RelayAutotune &at, uint32_t nowMs, int16_t currentTempCx100) {
  at.active = true;
  at.startMs = nowMs;
  at.cyclesDone = 0;
  at.periodsDone = 0;
  at.sumHighC = 0.0f;
  at.sumLowC = 0.0f;
  at.sumPeriodSec = 0.0f;
  at.lastLowCrossMs = 0;

  at.relayHigh = (currentTempCx100 < at.setpointCx100);
  at.cycleMaxCx100 = currentTempCx100;
  at.cycleMinCx100 = currentTempCx100;
}

uint8_t autotuneUpdate(
  RelayAutotune &at,
  int16_t tempCx100,
  uint32_t nowMs,
  bool &done,
  bool &ok,
  float &Ku,
  float &Pu
) {
  done = false;
  ok = false;
  Ku = 0.0f;
  Pu = 0.0f;

  if (!at.active) return 0;

  if ((nowMs - at.startMs) > at.timeoutMs) {
    at.active = false;
    done = true;
    ok = false;
    return 0;
  }

  const int16_t hiThresh = at.setpointCx100 + at.bandCx100;
  const int16_t loThresh = at.setpointCx100 - at.bandCx100;

  if (at.relayHigh) {
    if (tempCx100 > at.cycleMaxCx100) at.cycleMaxCx100 = tempCx100;

    if (tempCx100 >= hiThresh) {
      at.relayHigh = false;
      at.cycleMinCx100 = tempCx100;
      return 0;
    }
    return at.heatPwm;
  } else {
    if (tempCx100 < at.cycleMinCx100) at.cycleMinCx100 = tempCx100;

    if (tempCx100 <= loThresh) {
      at.sumHighC += (at.cycleMaxCx100 / 100.0f);
      at.sumLowC  += (at.cycleMinCx100 / 100.0f);
      at.cyclesDone++;

      if (at.lastLowCrossMs != 0) {
        at.sumPeriodSec += (nowMs - at.lastLowCrossMs) / 1000.0f;
        at.periodsDone++;
      }

      at.lastLowCrossMs = nowMs;

      if (at.cyclesDone >= at.maxCycles && at.periodsDone > 0) {
        float highAvgC = at.sumHighC / at.cyclesDone;
        float lowAvgC  = at.sumLowC  / at.cyclesDone;
        float PuSec    = at.sumPeriodSec / at.periodsDone;

        float a = 0.5f * (highAvgC - lowAvgC);
        float d = 0.5f * at.heatPwm;

        bool usable = (a > 0.10f && PuSec > 1.0f);

        if (usable) {
          Ku = (4.0f * d) / (PI * a);
          Pu = PuSec;
          ok = true;
        } else {
          ok = false;
        }

        at.active = false;
        done = true;
        return 0;
      }

      at.relayHigh = true;
      at.cycleMaxCx100 = tempCx100;
      return at.heatPwm;
    }

    return 0;
  }
}

void znPidFromKuPu(float Ku, float Pu, float &Kp, float &Ki, float &Kd) {
  Kp = 0.60f * Ku;
  Ki = (Pu > 0.0f) ? (2.0f * Kp / Pu) : 0.0f;
  Kd = (Kp * Pu) / 8.0f;

  Kp = clampf(Kp, 0.05f, 200.0f);
  Ki = clampf(Ki, 0.0f, 50.0f);
  Kd = clampf(Kd, 0.0f, 1000.0f);
}