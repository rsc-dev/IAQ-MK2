#pragma once
#include <stdint.h>
#include "readings.h"

// Wrap-safe bookkeeping shared by the sensor inputs: when did the last valid
// sample arrive, has the warm-up period since boot elapsed (latched once, so the
// 49.7-day millis() wrap cannot re-enter warm-up), and the resulting SensorState.
struct Readiness {
  bool everValid = false;
  bool warmedUp = false;
  uint32_t lastValidMs = 0;
};

inline void readinessMarkValid(Readiness& r, uint32_t nowMs) {
  r.everValid = true;
  r.lastValidMs = nowMs;
}

// `bootMs` is the millis() value when the sensor was powered/started; `plausible`
// lets a caller demote a value that is in range but not trustworthy yet.
SensorState readinessState(Readiness& r, uint32_t nowMs, uint32_t bootMs, uint32_t warmupMs,
                           uint32_t staleMs, bool plausible = true);

// Seconds since boot, counting millis() wraps. Call at least once per wrap.
struct UptimeCounter {
  uint32_t lastMs = 0;
  uint32_t wraps = 0;

  uint32_t seconds(uint32_t nowMs) {
    if (nowMs < lastMs) ++wraps;
    lastMs = nowMs;
    const uint64_t totalMs = ((uint64_t)wraps << 32) + nowMs;
    return (uint32_t)(totalMs / 1000u);
  }
};
