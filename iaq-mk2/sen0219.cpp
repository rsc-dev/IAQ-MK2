#include "sen0219.h"

namespace {
constexpr uint32_t kLeadTrailUs = 2000;                           // 2 ms high lead + 2 ms low trail
constexpr uint32_t kMinPeriodUs = kCo2PeriodUs / 10 * 9;          // -10 %
constexpr uint32_t kMaxPeriodUs = kCo2PeriodUs / 10 * 11;         // +10 %
}  // namespace

Co2Result co2FromPulse(uint32_t highUs, uint32_t lowUs) {
  const uint32_t periodUs = highUs + lowUs;
  if (periodUs < kMinPeriodUs || periodUs > kMaxPeriodUs) return {false, 0};
  if (highUs < kLeadTrailUs || lowUs < kLeadTrailUs) return {false, 0};

  // Integer arithmetic: numerator fits in 64 bits comfortably.
  const uint64_t numerator = (uint64_t)kCo2RangePpm * (highUs - kLeadTrailUs);
  const uint32_t denominator = periodUs - 2 * kLeadTrailUs;
  const uint32_t ppm = (uint32_t)((numerator + denominator / 2) / denominator);
  if (ppm > kCo2RangePpm) return {false, 0};
  return {true, (uint16_t)ppm};
}
