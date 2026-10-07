#include <fstream>
#include <sstream>
#include <string>
#include "test.h"
#include "forecast.h"
#include "host_sources.h"
#include "readings.h"

static std::string fixture() {
  std::ifstream f("fixtures/gdynia_compact.json");
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

TEST(forecast_extracts_first_timeseries_entry_from_fixture) {
  StringSource src(fixture());
  char entry[1536];
  CHECK(forecastExtractFirstEntry(src, entry, sizeof entry));
  CHECK(std::string(entry).find("2026-10-07T05:00:00Z") != std::string::npos);
  CHECK(std::string(entry).find("next_6_hours") != std::string::npos);
  CHECK(std::string(entry).back() == '}');
}

TEST(forecast_parses_fixture_entry) {
  StringSource src(fixture());
  char entry[1536];
  CHECK(forecastExtractFirstEntry(src, entry, sizeof entry));
  OutdoorReading o;
  CHECK(forecastParseEntry(entry, o));
  CHECK_NEAR(o.tempC, 9.8, 0.01);
  CHECK_NEAR(o.windMs, 2.9, 0.01);
  CHECK_STR_EQ(o.symbol, "p.cloudy");
  CHECK_NEAR(o.rainNext6hMm, 0.0, 0.01);
}

TEST(forecast_rejects_truncated_and_garbage) {
  std::string cut = fixture().substr(0, 400);                     // ends inside the first entry
  StringSource a(cut);
  char entry[1536];
  CHECK(!forecastExtractFirstEntry(a, entry, sizeof entry));
  StringSource b("{\"properties\":{\"meta\":{}}}");               // no timeseries at all
  CHECK(!forecastExtractFirstEntry(b, entry, sizeof entry));
  OutdoorReading o;
  CHECK(!forecastParseEntry("{\"time\":\"x\",\"data\":{}}", o));   // fields missing
  CHECK(!forecastParseEntry("not json", o));
}

TEST(forecast_entry_too_large_for_buffer_fails) {
  StringSource src(fixture());
  char small[64];
  CHECK(!forecastExtractFirstEntry(src, small, sizeof small));
}

TEST(forecast_symbol_words) {
  CHECK_STR_EQ(forecastSymbolWord("partlycloudy_day"), "p.cloudy");
  CHECK_STR_EQ(forecastSymbolWord("clearsky_night"), "clear");
  CHECK_STR_EQ(forecastSymbolWord("fair_polartwilight"), "fair");
  CHECK_STR_EQ(forecastSymbolWord("cloudy"), "cloudy");
  CHECK_STR_EQ(forecastSymbolWord("fog"), "fog");
  CHECK_STR_EQ(forecastSymbolWord("lightrainshowers_day"), "lt.rain");
  CHECK_STR_EQ(forecastSymbolWord("rain"), "rain");
  CHECK_STR_EQ(forecastSymbolWord("heavyrain"), "hvy rain");
  CHECK_STR_EQ(forecastSymbolWord("heavyrainandthunder"), "storm");
  CHECK_STR_EQ(forecastSymbolWord("lightsleet"), "sleet");
  CHECK_STR_EQ(forecastSymbolWord("heavysnowshowers_night"), "hvy snow");
  CHECK_STR_EQ(forecastSymbolWord("snow"), "snow");
  CHECK_STR_EQ(forecastSymbolWord("somethingnew"), "?");
}
