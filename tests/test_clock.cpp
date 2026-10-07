#include "test.h"
#include "clock.h"

// Reference instants (UTC) -> Europe/Warsaw wall clock.
TEST(clock_cest_summer) {
  LocalTime t;
  CHECK(localTimeWarsaw(1791352452u, t));   // 2026-10-07T05:54:12Z
  CHECK_EQ(t.hour, 7);
  CHECK_EQ(t.minute, 54);
}

TEST(clock_cet_winter) {
  LocalTime t;
  CHECK(localTimeWarsaw(1796558400u, t));   // 2026-12-06T12:00:00Z
  CHECK_EQ(t.hour, 13);
  CHECK_EQ(t.minute, 0);
}

TEST(clock_dst_starts_last_sunday_of_march) {
  LocalTime t;
  CHECK(localTimeWarsaw(1774745999u, t));   // 2026-03-29T00:59:59Z -> 01:59 CET
  CHECK_EQ(t.hour, 1);
  CHECK_EQ(t.minute, 59);
  CHECK(localTimeWarsaw(1774746000u, t));   // 2026-03-29T01:00:00Z -> 03:00 CEST
  CHECK_EQ(t.hour, 3);
  CHECK_EQ(t.minute, 0);
}

TEST(clock_dst_ends_last_sunday_of_october) {
  LocalTime t;
  CHECK(localTimeWarsaw(1792890000u - 1u, t));   // 2026-10-25T00:59:59Z -> 02:59 CEST
  CHECK_EQ(t.hour, 2);
  CHECK_EQ(t.minute, 59);
  CHECK(localTimeWarsaw(1792890000u, t));        // 2026-10-25T01:00:00Z -> 02:00 CET
  CHECK_EQ(t.hour, 2);
  CHECK_EQ(t.minute, 0);
}

TEST(clock_formats_hhmm_or_placeholder) {
  char b[8];
  formatClockWarsaw(b, sizeof b, 1791352452u);
  CHECK_STR_EQ(b, "07:54");
  formatClockWarsaw(b, sizeof b, 0u);
  CHECK_STR_EQ(b, "--:--");
  LocalTime t;
  CHECK(!localTimeWarsaw(0u, t));
}
