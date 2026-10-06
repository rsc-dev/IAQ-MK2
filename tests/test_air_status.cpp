#include "test.h"
#include "air_status.h"

static Co2Reading co2(uint16_t ppm, SensorState s = SensorState::Ok) { Co2Reading r; r.ppm = ppm; r.state = s; return r; }
static PmReading pm25(uint16_t v, SensorState s = SensorState::Ok) { PmReading r; r.pm25 = v; r.state = s; return r; }
static EnsReading aqi(uint8_t v, SensorState s = SensorState::Ok) { EnsReading r; r.aqi = v; r.state = s; return r; }

TEST(co2_thresholds) {
  CHECK(co2Level(co2(799)) == Level::Good);
  CHECK(co2Level(co2(800)) == Level::Warn);
  CHECK(co2Level(co2(1199)) == Level::Warn);
  CHECK(co2Level(co2(1200)) == Level::Bad);
  CHECK(co2Level(co2(5000)) == Level::Bad);
}

TEST(pm25_thresholds) {
  CHECK(pm25Level(pm25(0)) == Level::Good);
  CHECK(pm25Level(pm25(15)) == Level::Good);
  CHECK(pm25Level(pm25(16)) == Level::Warn);
  CHECK(pm25Level(pm25(35)) == Level::Warn);
  CHECK(pm25Level(pm25(36)) == Level::Bad);
}

TEST(aqi_thresholds) {
  CHECK(aqiLevel(aqi(1)) == Level::Good);
  CHECK(aqiLevel(aqi(2)) == Level::Warn);
  CHECK(aqiLevel(aqi(3)) == Level::Bad);
  CHECK(aqiLevel(aqi(5)) == Level::Bad);
}

TEST(non_ok_states_are_unknown) {
  CHECK(co2Level(co2(2000, SensorState::WarmingUp)) == Level::Unknown);
  CHECK(co2Level(co2(2000, SensorState::Stale)) == Level::Unknown);
  CHECK(pm25Level(pm25(100, SensorState::NotReady)) == Level::Unknown);
  CHECK(aqiLevel(aqi(5, SensorState::Unavailable)) == Level::Unknown);
}

TEST(overall_is_worst_of_valid_metrics) {
  Readings r;
  CHECK(overallLevel(r) == Level::Unknown);           // nothing valid yet
  r.pm = pm25(5);
  CHECK(overallLevel(r) == Level::Good);              // CO2 still warming up, PM is fine
  r.co2 = co2(900);
  CHECK(overallLevel(r) == Level::Warn);
  r.ens = aqi(4);
  CHECK(overallLevel(r) == Level::Bad);
  r.ens.state = SensorState::Stale;                   // stale sensor no longer counts
  CHECK(overallLevel(r) == Level::Warn);
}

TEST(level_names) {
  CHECK_STR_EQ(levelName(Level::Unknown), "WARMUP");
  CHECK_STR_EQ(levelName(Level::Good), "GOOD");
  CHECK_STR_EQ(levelName(Level::Warn), "WARN");
  CHECK_STR_EQ(levelName(Level::Bad), "BAD");
}
