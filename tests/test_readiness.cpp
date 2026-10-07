#include "test.h"
#include "readiness.h"

namespace {
constexpr uint32_t kWarm = 180000;
constexpr uint32_t kStale = 5000;
}

TEST(readiness_before_first_sample_is_not_ready) {
  Readiness r;
  CHECK(readinessState(r, 1000, 0, kWarm, kStale) == SensorState::NotReady);
}

TEST(readiness_warms_up_then_ok_then_stale_then_recovers) {
  Readiness r;
  readinessMarkValid(r, 2000);
  CHECK(readinessState(r, 2000, 0, kWarm, kStale) == SensorState::WarmingUp);
  readinessMarkValid(r, 180000);
  CHECK(readinessState(r, 180000, 0, kWarm, kStale) == SensorState::Ok);
  CHECK(readinessState(r, 180000 + kStale + 1, 0, kWarm, kStale) == SensorState::Stale);
  readinessMarkValid(r, 200000);
  CHECK(readinessState(r, 200000, 0, kWarm, kStale) == SensorState::Ok);  // no second warm-up
}

TEST(readiness_implausible_sample_reads_as_warm_up) {
  Readiness r;
  readinessMarkValid(r, 300000);
  CHECK(readinessState(r, 300000, 0, kWarm, kStale, false) == SensorState::WarmingUp);
  CHECK(readinessState(r, 300000, 0, kWarm, kStale, true) == SensorState::Ok);
}

TEST(readiness_survives_millis_wrap) {
  Readiness r;
  const uint32_t boot = 0;
  readinessMarkValid(r, 0xFFFFFFF8u);                                   // 49.7 days in
  CHECK(readinessState(r, 0xFFFFFFF8u, boot, kWarm, kStale) == SensorState::Ok);
  readinessMarkValid(r, 10u);                                            // just after the wrap
  CHECK(readinessState(r, 10u, boot, kWarm, kStale) == SensorState::Ok); // must not re-enter warm-up
  CHECK(readinessState(r, 10u + kStale + 1, boot, kWarm, kStale) == SensorState::Stale);
}

TEST(readiness_warm_up_counts_from_boot_time) {
  Readiness r;
  const uint32_t boot = 0xFFFFF000u;                                     // sensor powered just before a wrap
  readinessMarkValid(r, 0xFFFFF100u);
  CHECK(readinessState(r, 0xFFFFF100u, boot, kWarm, kStale) == SensorState::WarmingUp);
  readinessMarkValid(r, boot + kWarm);                                   // wraps past zero
  CHECK(readinessState(r, boot + kWarm, boot, kWarm, kStale) == SensorState::Ok);
}

TEST(uptime_counts_across_millis_wrap) {
  UptimeCounter u;
  CHECK_EQ(u.seconds(0), 0);
  CHECK_EQ(u.seconds(90000), 90);
  CHECK_EQ(u.seconds(0xFFFFFF00u), 4294967);
  CHECK_EQ(u.seconds(0x100u), 4294967);                                  // 4294967296 + 256 ms
  CHECK_EQ(u.seconds(0x100u + 60000), 4294967 + 60);
}
