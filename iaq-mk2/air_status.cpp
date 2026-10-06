#include "air_status.h"

#include "config.h"

Level co2Level(const Co2Reading& r) {
  if (r.state != SensorState::Ok) return Level::Unknown;
  if (r.ppm >= cfg::CO2_BAD_PPM) return Level::Bad;
  if (r.ppm >= cfg::CO2_WARN_PPM) return Level::Warn;
  return Level::Good;
}

Level pm25Level(const PmReading& r) {
  if (r.state != SensorState::Ok) return Level::Unknown;
  if (r.pm25 >= cfg::PM25_BAD_UGM3) return Level::Bad;
  if (r.pm25 >= cfg::PM25_WARN_UGM3) return Level::Warn;
  return Level::Good;
}

Level aqiLevel(const EnsReading& r) {
  if (r.state != SensorState::Ok) return Level::Unknown;
  if (r.aqi >= cfg::AQI_BAD) return Level::Bad;
  if (r.aqi >= cfg::AQI_WARN) return Level::Warn;
  return Level::Good;
}

Level overallLevel(const Readings& r) {
  const Level levels[] = {co2Level(r.co2), pm25Level(r.pm), aqiLevel(r.ens)};
  Level worst = Level::Unknown;
  for (const Level l : levels) {
    if (l == Level::Unknown) continue;
    if (worst == Level::Unknown || l > worst) worst = l;
  }
  return worst;
}

const char* levelName(Level level) {
  switch (level) {
    case Level::Good: return "GOOD";
    case Level::Warn: return "WARN";
    case Level::Bad:  return "BAD";
    case Level::Unknown: break;
  }
  return "WARMUP";
}
