// Renders dashboard scenarios to PPM files so the layout can be checked on the host.
#include <cstdio>
#include <cstring>
#include <string>

#include "dashboard.h"
#include "readings.h"

namespace {
const uint8_t kPalette[4][3] = {{0, 0, 0}, {255, 255, 255}, {250, 200, 0}, {220, 30, 30}};

void writePpm(const std::string& path, const uint8_t* fb) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::perror(path.c_str()); return; }
  std::fprintf(f, "P6\n296 128\n255\n");
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 296; ++x) std::fwrite(kPalette[dashboardPixel(fb, x, y) & 3], 1, 3, f);
  std::fclose(f);
  std::printf("wrote %s\n", path.c_str());
}

Readings good() {
  Readings r;
  r.co2 = {684, SensorState::Ok};
  r.pm = {5, 8, 11, SensorState::Ok};
  r.ens.aqi = 1; r.ens.tvocPpb = 42; r.ens.eco2Ppm = 612; r.ens.state = SensorState::Ok;
  r.env = {22.6f, 46.0f, 1012.3f, 86.2f, SensorState::Ok};
  r.outdoor.tempC = 9.8f; r.outdoor.windMs = 2.9f; r.outdoor.rainNext6hMm = 0.0f;
  std::strcpy(r.outdoor.symbol, "p.cloudy");
  r.outdoor.state = SensorState::Ok;
  r.time.epoch = 1791352452u; r.time.valid = true;   // 07:54 CEST
  return r;
}
}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "out";
  static uint8_t fb[kFrameBufferSize];

  Readings r = good();
  dashboardRender(r, fb);
  writePpm(dir + "/01_good.ppm", fb);

  r = good(); r.co2.ppm = 950; r.pm.pm25 = 22; r.ens.aqi = 2;
  dashboardRender(r, fb);
  writePpm(dir + "/02_warn.ppm", fb);

  r = good(); r.co2.ppm = 1850; r.pm.pm25 = 48; r.pm.pm10 = 61; r.ens.aqi = 4; r.ens.tvocPpb = 1200;
  r.outdoor.tempC = 6.0f; r.outdoor.windMs = 9.0f; r.outdoor.rainNext6hMm = 4.2f; std::strcpy(r.outdoor.symbol, "rain");
  dashboardRender(r, fb);
  writePpm(dir + "/03_bad.ppm", fb);

  Readings w;                                   // boot screen
  w.pm.state = SensorState::NotReady;
  w.env = {21.9f, 51.0f, 1008.7f, 12.0f, SensorState::Ok};
  dashboardRender(w, fb);
  writePpm(dir + "/04_boot.ppm", fb);

  r = good(); r.co2.state = SensorState::Stale; r.pm.state = SensorState::WarmingUp;
  r.ens.state = SensorState::WarmingUp; r.ens.validityFlag = 2; r.env.state = SensorState::Unavailable;
  dashboardRender(r, fb);
  writePpm(dir + "/05_degraded.ppm", fb);

  r = good(); r.co2.ppm = 5000; r.pm = {999, 999, 999, SensorState::Ok}; r.ens.aqi = 5; r.ens.tvocPpb = 65000;
  r.env = {-10.5f, 100.0f, 1099.9f, 9999.0f, SensorState::Ok};
  dashboardRender(r, fb);
  writePpm(dir + "/06_extreme.ppm", fb);
  return 0;
}
