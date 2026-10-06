#pragma once
#include <stdint.h>

// DFRobot SEN0219 (Winsen MH-Z14-class NDIR) PWM output:
//   cycle 1004 ms +-5 %, 2 ms high at the start and 2 ms low at the end,
//   ppm = 5000 * (TH - 2 ms) / (TH + TL - 4 ms).
// Pure conversion from one measured cycle to ppm with plausibility checks.

constexpr uint32_t kCo2PeriodUs = 1004000;
constexpr uint16_t kCo2RangePpm = 5000;

struct Co2Result {
  bool valid;
  uint16_t ppm;
};

Co2Result co2FromPulse(uint32_t highUs, uint32_t lowUs);
