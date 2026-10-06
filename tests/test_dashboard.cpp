#include <cstring>
#include "test.h"
#include "dashboard.h"
#include "readings.h"

namespace {
constexpr size_t kCanary = 64;
struct GuardedBuffer {
  uint8_t bytes[kFrameBufferSize + kCanary];
  GuardedBuffer() { std::memset(bytes, 0xA5, sizeof bytes); }
  bool canaryIntact() const {
    for (size_t i = kFrameBufferSize; i < sizeof bytes; ++i) if (bytes[i] != 0xA5) return false;
    return true;
  }
};

Readings extreme() {
  Readings r;
  r.co2.ppm = 5000; r.co2.state = SensorState::Ok;
  r.pm.pm1 = 999; r.pm.pm25 = 999; r.pm.pm10 = 999; r.pm.state = SensorState::Ok;
  r.ens.aqi = 5; r.ens.tvocPpb = 65000; r.ens.eco2Ppm = 65000; r.ens.state = SensorState::Ok;
  r.env.temperatureC = -10.5f; r.env.humidityPct = 100.0f; r.env.pressureHpa = 1099.9f; r.env.gasKOhm = 9999.0f;
  r.env.state = SensorState::Ok;
  return r;
}

int countColour(const uint8_t* fb, uint8_t colour) {
  int n = 0;
  for (int y = 0; y < 128; ++y) for (int x = 0; x < 296; ++x) if (dashboardPixel(fb, x, y) == colour) ++n;
  return n;
}
}  // namespace

TEST(dashboard_extreme_values_stay_in_bounds) {
  GuardedBuffer g;
  dashboardRender(extreme(), 99 * 86400 + 23 * 3600, g.bytes);
  CHECK(g.canaryIntact());
  CHECK(countColour(g.bytes, 0) > 500);          // black ink present
  CHECK(countColour(g.bytes, 3) > 50);           // red: BAD badge and values
}

TEST(dashboard_good_has_no_warning_colours) {
  Readings r;
  r.co2.ppm = 650; r.co2.state = SensorState::Ok;
  r.pm.pm1 = 3; r.pm.pm25 = 5; r.pm.pm10 = 7; r.pm.state = SensorState::Ok;
  r.ens.aqi = 1; r.ens.tvocPpb = 40; r.ens.state = SensorState::Ok;
  r.env.temperatureC = 22.6f; r.env.humidityPct = 46.0f; r.env.pressureHpa = 1012.3f; r.env.gasKOhm = 86.2f;
  r.env.state = SensorState::Ok;
  GuardedBuffer g;
  dashboardRender(r, 3 * 3600 + 12 * 60, g.bytes);
  CHECK(g.canaryIntact());
  CHECK_EQ(countColour(g.bytes, 2), 0);
  CHECK_EQ(countColour(g.bytes, 3), 0);
}

TEST(dashboard_warmup_renders_without_valid_data) {
  Readings r;                                     // everything NotReady
  r.env.state = SensorState::Unavailable;
  GuardedBuffer g;
  dashboardRender(r, 5, g.bytes);
  CHECK(g.canaryIntact());
  CHECK(countColour(g.bytes, 0) > 500);
}

TEST(dashboard_pixel_decoder_matches_rotation) {
  GuardedBuffer g;
  Readings r;
  dashboardRender(r, 0, g.bytes);
  // Header rule at y = 19 is black across the full width; row 60 near the
  // right edge is white in the warm-up screen.
  CHECK_EQ(dashboardPixel(g.bytes, 0, 19), 0);
  CHECK_EQ(dashboardPixel(g.bytes, 295, 19), 0);
  CHECK_EQ(dashboardPixel(g.bytes, 295, 60), 1);
}
