#include "test.h"
#include "sen0219.h"

TEST(co2_datasheet_formula) {
  // ppm = 5000 * (TH - 2 ms) / (TH + TL - 4 ms), cycle 1004 ms
  Co2Result r = co2FromPulse(2000 + 400 * 200, 1004000 - (2000 + 400 * 200));
  CHECK(r.valid);
  CHECK_EQ(r.ppm, 400);
  r = co2FromPulse(2000 + 1000 * 200, 1004000 - (2000 + 1000 * 200));
  CHECK(r.valid);
  CHECK_EQ(r.ppm, 1000);
}

TEST(co2_endpoints) {
  CHECK_EQ(co2FromPulse(2000, 1002000).ppm, 0);        // TH = 2 ms -> 0 ppm
  CHECK_EQ(co2FromPulse(1002000, 2000).ppm, 5000);     // TL = 2 ms -> 5000 ppm
  CHECK(co2FromPulse(1002000, 2000).valid);
}

TEST(co2_tolerates_period_jitter) {
  // Same duty at a 4 % slow cycle still converts
  const uint32_t period = 1044000;
  const uint32_t high = 2000 + (uint32_t)((period - 4000) * 0.16);     // ~800 ppm
  Co2Result r = co2FromPulse(high, period - high);
  CHECK(r.valid);
  CHECK(r.ppm >= 795 && r.ppm <= 805);
}

TEST(co2_rejects_implausible_cycles) {
  CHECK(!co2FromPulse(100000, 100000).valid);          // 200 ms period
  CHECK(!co2FromPulse(1000000, 1000000).valid);        // 2 s period
  CHECK(!co2FromPulse(0, 1004000).valid);              // no high pulse
  CHECK(!co2FromPulse(1500, 1002500).valid);           // TH < 2 ms lead pulse
  CHECK(!co2FromPulse(1004000, 0).valid);              // stuck high
}

TEST(co2_wraparound_safe_inputs) {
  // Callers compute durations with unsigned subtraction; verify a duration that
  // crossed the micros() wrap still converts when passed as plain durations.
  const uint32_t riseUs = 0xFFFFFF00u, fallUs = riseUs + 202000u;  // wraps
  const uint32_t highUs = fallUs - riseUs;
  CHECK_EQ(highUs, 202000u);
  CHECK(co2FromPulse(highUs, 1004000 - highUs).valid);
}
