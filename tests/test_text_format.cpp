#include <cmath>
#include "test.h"
#include "text_format.h"

TEST(formatFixed_rounds_and_pads) {
  char b[16];
  formatFixed(b, sizeof b, 22.56f, 1);  CHECK_STR_EQ(b, "22.6");
  formatFixed(b, sizeof b, 1012.34f, 1); CHECK_STR_EQ(b, "1012.3");
  formatFixed(b, sizeof b, 46.0f, 0);   CHECK_STR_EQ(b, "46");
  formatFixed(b, sizeof b, 0.05f, 1);   CHECK_STR_EQ(b, "0.1");
  formatFixed(b, sizeof b, 86.249f, 2); CHECK_STR_EQ(b, "86.25");
  formatFixed(b, sizeof b, 5.0f, 3);    CHECK_STR_EQ(b, "5.000");
}

TEST(formatFixed_negative_nan_inf) {
  char b[16];
  formatFixed(b, sizeof b, -3.5f, 1);    CHECK_STR_EQ(b, "-3.5");
  formatFixed(b, sizeof b, -0.04f, 1);   CHECK_STR_EQ(b, "0.0");   // rounds to zero: no minus sign
  formatFixed(b, sizeof b, NAN, 1);      CHECK_STR_EQ(b, "--");
  formatFixed(b, sizeof b, INFINITY, 0); CHECK_STR_EQ(b, "--");
}

TEST(formatFixed_truncates_to_buffer) {
  char b[4];
  const size_t n = formatFixed(b, sizeof b, 1234.5f, 1);
  CHECK_EQ(n, 3);
  CHECK_STR_EQ(b, "123");
  CHECK_EQ(formatFixed(b, 0, 1.0f, 0), 0);
}

TEST(formatUptime_units) {
  char b[16];
  formatUptime(b, sizeof b, 0);              CHECK_STR_EQ(b, "0m");
  formatUptime(b, sizeof b, 59);             CHECK_STR_EQ(b, "0m");
  formatUptime(b, sizeof b, 12 * 60);        CHECK_STR_EQ(b, "12m");
  formatUptime(b, sizeof b, 3 * 3600 + 720); CHECK_STR_EQ(b, "3h12m");
  formatUptime(b, sizeof b, 3 * 3600 + 60);  CHECK_STR_EQ(b, "3h01m");
  formatUptime(b, sizeof b, 2 * 86400 + 3 * 3600); CHECK_STR_EQ(b, "2d03h");
}

TEST(formatUInt_min_digits) {
  char b[16];
  formatUInt(b, sizeof b, 7, 2);     CHECK_STR_EQ(b, "07");
  formatUInt(b, sizeof b, 0, 1);     CHECK_STR_EQ(b, "0");
  formatUInt(b, sizeof b, 65000, 0); CHECK_STR_EQ(b, "65000");
}
