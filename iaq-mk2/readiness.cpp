#include "readiness.h"

SensorState readinessState(Readiness& r, uint32_t nowMs, uint32_t bootMs, uint32_t warmupMs,
                           uint32_t staleMs, bool plausible) {
  if (!r.warmedUp && nowMs - bootMs >= warmupMs) r.warmedUp = true;
  if (!r.everValid) return SensorState::NotReady;
  if (nowMs - r.lastValidMs > staleMs) return SensorState::Stale;
  if (!r.warmedUp || !plausible) return SensorState::WarmingUp;
  return SensorState::Ok;
}
