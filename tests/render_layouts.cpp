// Layout mockups for the 296 x 128 black/white/red panel, rendered with the
// real fonts and primitives. Host-only: `make -C tests layouts`.
#include <cstdio>
#include <cstring>
#include <string>

#include "air_status.h"
#include "dashboard.h"
#include "draw.h"
#include "readings.h"
#include "text_format.h"

using namespace draw;

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

uint8_t ink(Level l) { return l == Level::Bad ? kRed : kBlack; }
void warnMark(Level l, int x, int y, int w, int thick) { if (l == Level::Warn) fillRect(x, y, x + w - 1, y + thick - 1, kRed); }

const char* aqiWord(uint8_t aqi) {
  switch (aqi) { case 1: return "excellent"; case 2: return "good"; case 3: return "moderate"; case 4: return "poor"; case 5: return "unhealthy"; }
  return "?";
}

void header(const Readings& r, uint32_t up, bool withBadge) {
  text(4, 1, "AIR QUALITY", Font16, kBlack);
  char u[20], t[28] = "up ";
  formatUptime(u, sizeof u, up);
  strncat(t, u, sizeof t - 4);
  if (!withBadge) { textRight(292, 4, t, Font12, kBlack); return; }
  const Level o = overallLevel(r);
  const char* s = levelName(o);
  const int left = 292 - textWidth(s, Font16) - 9;
  uint8_t fill = kWhite, fg = kBlack;
  if (o == Level::Bad) { fill = kRed; fg = kWhite; fillRect(left, 0, 292, 17, kRed); }
  else if (o == Level::Warn) { outlineRect(left, 0, 292, 17, kRed); outlineRect(left + 1, 1, 291, 16, kRed); }
  else if (o == Level::Good) outlineRect(left, 0, 292, 17, kBlack);
  text(left + 5, 1, s, Font16, fg, fill);
  textRight(left - 8, 4, t, Font12, kBlack);
}

void tempText(int x, int y, const sFONT& f, float c) {
  char n[12];
  formatFixed(n, sizeof n, c, 1);
  text(x, y, n, f, kBlack);
  const int dx = x + textWidth(n, f) + 3;
  degree(dx, y + 2, kBlack);
  text(dx + 5, y, "C", f, kBlack);
}

// Outdoor forecast as the weather module would provide it (design mockup).
struct Outdoor {
  bool valid;
  float tempC;
  float windMs;
  const char* symbol;      // short word for the met.no symbol code
  float rainNext6hMm;
  const char* updated;     // HH:MM from NTP
};

// One line of forecast. withPlace prefixes "Gdynia 10°C"; the credit goes to the right in Font8.
void outdoorLine(int y, const Outdoor& o, bool withPlace, bool credit) {
  if (!o.valid) { text(6, y, withPlace ? "Gdynia: no forecast" : "no forecast", Font12, kBlack); return; }
  char n[12], l[48];
  int x = 6;
  if (withPlace) {
    formatFixed(n, sizeof n, o.tempC, 0);
    std::snprintf(l, sizeof l, "Gdynia %s", n);
    text(x, y, l, Font12, kBlack);
    x += textWidth(l, Font12) + 2;
    degree(x, y + 1, kBlack);
    text(x + 4, y, "C", Font12, kBlack);
    x += 4 + 7 + 10;
  }
  formatFixed(n, sizeof n, o.windMs, 0);
  std::snprintf(l, sizeof l, "%s  %s m/s", o.symbol, n);
  text(x, y, l, Font12, kBlack);
  x += textWidth(l, Font12) + 10;
  formatFixed(n, sizeof n, o.rainNext6hMm, 1);
  std::snprintf(l, sizeof l, "6h %s mm", n);
  text(x, y, l, Font12, o.rainNext6hMm >= 0.5f ? kRed : kBlack);
  if (credit) textRight(290, y + 3, "MET Norway", Font8, kBlack);
}

void headerWithTime(const Readings& r, const char* hhmm) {
  text(4, 1, "AIR QUALITY", Font16, kBlack);
  const Level o = overallLevel(r);
  const char* s = levelName(o);
  const int left = 292 - textWidth(s, Font16) - 9;
  uint8_t fill = kWhite, fg = kBlack;
  if (o == Level::Bad) { fill = kRed; fg = kWhite; fillRect(left, 0, 292, 17, kRed); }
  else if (o == Level::Warn) { outlineRect(left, 0, 292, 17, kRed); outlineRect(left + 1, 1, 291, 16, kRed); }
  else if (o == Level::Good) outlineRect(left, 0, 292, 17, kBlack);
  text(left + 5, 1, s, Font16, fg, fill);
  textRight(left - 8, 1, hhmm, Font16, kBlack);
}

// ---------------------------------------------------------------- W1: current layout + outdoor line
void layoutOutdoorLine(const Readings& r, const Outdoor& o, uint8_t* fb) {
  beginFrame(fb);
  headerWithTime(r, o.updated);
  hLine(0, 295, 19);
  vLine(147, 21, 64);
  text(6, 23, "CO2", Font12, kBlack);
  text(154, 23, "PM2.5", Font12, kBlack);
  char v[12];
  if (r.co2.state == SensorState::Ok) { formatUInt(v, sizeof v, r.co2.ppm, 1); text(6, 37, v, Font24, ink(co2Level(r.co2))); warnMark(co2Level(r.co2), 6, 62, textWidth(v, Font24), 2); text(6 + textWidth(v, Font24) + 5, 49, "ppm", Font12, kBlack); }
  else text(6, 37, "--", Font24, kBlack);
  if (r.pm.state == SensorState::Ok) { formatUInt(v, sizeof v, r.pm.pm25, 1); text(154, 37, v, Font24, ink(pm25Level(r.pm))); warnMark(pm25Level(r.pm), 154, 62, textWidth(v, Font24), 2); text(154 + textWidth(v, Font24) + 5, 49, "ug/m3", Font12, kBlack); }
  else text(154, 37, "--", Font24, kBlack);
  hLine(0, 295, 66);
  char line[40];
  if (r.pm.state == SensorState::Ok) std::snprintf(line, sizeof line, "PM1 %u  PM10 %u", r.pm.pm1, r.pm.pm10); else std::snprintf(line, sizeof line, "PM1 --  PM10 --");
  text(6, 70, line, Font12, kBlack);
  if (r.ens.state == SensorState::Ok) {
    text(154, 70, "AQI ", Font12, kBlack);
    formatUInt(v, sizeof v, r.ens.aqi, 1);
    text(182, 70, v, Font12, ink(aqiLevel(r.ens)));
    warnMark(aqiLevel(r.ens), 182, 82, 7, 1);
    std::snprintf(line, sizeof line, "TVOC %u ppb", r.ens.tvocPpb);
    text(196, 70, line, Font12, kBlack);
  } else text(154, 70, "AQI -  TVOC --", Font12, kBlack);
  if (r.env.state == SensorState::Ok) {
    char n[12];
    tempText(6, 84, Font12, r.env.temperatureC);
    formatFixed(n, sizeof n, r.env.humidityPct, 0); std::snprintf(line, sizeof line, "%s %%", n); text(62, 84, line, Font12, kBlack);
    formatFixed(n, sizeof n, r.env.pressureHpa, 0); std::snprintf(line, sizeof line, "%s hPa", n); text(104, 84, line, Font12, kBlack);
    formatFixed(n, sizeof n, r.env.gasKOhm, 0); std::snprintf(line, sizeof line, "GAS %s kOhm", n); textRight(290, 84, line, Font12, kBlack);
  } else text(6, 84, "BME680 --", Font12, kBlack);
  hLine(0, 295, 98);
  outdoorLine(102, o, true, false);
  textRight(290, 118, "forecast: MET Norway", Font8, kBlack);
}

// ---------------------------------------------------------------- W2: three tiles, outdoor temperature as the third
void layoutOutdoorTile(const Readings& r, const Outdoor& o, uint8_t* fb) {
  beginFrame(fb);
  headerWithTime(r, o.updated);
  hLine(0, 295, 19);
  vLine(98, 22, 65); vLine(197, 22, 65);
  const int col[3] = {6, 104, 203};
  text(col[0], 23, "CO2 ppm", Font12, kBlack);
  text(col[1], 23, "PM2.5 ug/m3", Font12, kBlack);
  text(col[2], 23, "GDYNIA", Font12, kBlack);
  char v[12];
  if (r.co2.state == SensorState::Ok) { formatUInt(v, sizeof v, r.co2.ppm, 1); text(col[0], 37, v, Font24, ink(co2Level(r.co2))); warnMark(co2Level(r.co2), col[0], 62, textWidth(v, Font24), 2); }
  else text(col[0], 37, "--", Font24, kBlack);
  if (r.pm.state == SensorState::Ok) { formatUInt(v, sizeof v, r.pm.pm25, 1); text(col[1], 37, v, Font24, ink(pm25Level(r.pm))); warnMark(pm25Level(r.pm), col[1], 62, textWidth(v, Font24), 2); }
  else text(col[1], 37, "--", Font24, kBlack);
  if (o.valid) tempText(col[2], 37, Font24, o.tempC); else text(col[2], 37, "--", Font24, kBlack);
  hLine(0, 295, 68);
  if (r.env.state == SensorState::Ok) {
    char n[12], l[24];
    tempText(6, 73, Font16, r.env.temperatureC);
    formatFixed(n, sizeof n, r.env.humidityPct, 0); std::snprintf(l, sizeof l, "%s %%", n); text(118, 73, l, Font16, kBlack);
    formatFixed(n, sizeof n, r.env.pressureHpa, 0); std::snprintf(l, sizeof l, "%s hPa", n); textRight(290, 73, l, Font16, kBlack);
  } else text(6, 73, "BME680 --", Font16, kBlack);
  hLine(0, 295, 92);
  char line[48];
  if (r.ens.state == SensorState::Ok) {
    text(6, 96, "AQI ", Font12, kBlack);
    formatUInt(v, sizeof v, r.ens.aqi, 1);
    text(34, 96, v, Font12, ink(aqiLevel(r.ens)));
    warnMark(aqiLevel(r.ens), 34, 108, 7, 1);
    text(48, 96, aqiWord(r.ens.aqi), Font12, kBlack);
    std::snprintf(line, sizeof line, "TVOC %u  PM10 %s", r.ens.tvocPpb, "%u");
  } else text(6, 96, "AQI -", Font12, kBlack);
  {
    char tv[16] = "TVOC --", pm[16] = "PM10 --";
    if (r.ens.state == SensorState::Ok) std::snprintf(tv, sizeof tv, "TVOC %u", r.ens.tvocPpb);
    if (r.pm.state == SensorState::Ok) std::snprintf(pm, sizeof pm, "PM10 %u", r.pm.pm10);
    std::snprintf(line, sizeof line, "%s  %s", tv, pm);
    textRight(290, 96, line, Font12, kBlack);
  }
  outdoorLine(112, o, false, true);
}

// ---------------------------------------------------------------- B: three tiles
void layoutThreeTiles(const Readings& r, uint32_t up, uint8_t* fb) {
  beginFrame(fb);
  header(r, up, true);
  hLine(0, 295, 19);
  vLine(98, 22, 65); vLine(197, 22, 65);
  const int col[3] = {6, 104, 203};
  text(col[0], 23, "CO2 ppm", Font12, kBlack);
  text(col[1], 23, "PM2.5 ug/m3", Font12, kBlack);
  text(col[2], 23, "AQI 1-5", Font12, kBlack);
  char v[12];
  if (r.co2.state == SensorState::Ok) { formatUInt(v, sizeof v, r.co2.ppm, 1); text(col[0], 37, v, Font24, ink(co2Level(r.co2))); warnMark(co2Level(r.co2), col[0], 62, textWidth(v, Font24), 2); }
  else text(col[0], 37, "--", Font24, kBlack);
  if (r.pm.state == SensorState::Ok) { formatUInt(v, sizeof v, r.pm.pm25, 1); text(col[1], 37, v, Font24, ink(pm25Level(r.pm))); warnMark(pm25Level(r.pm), col[1], 62, textWidth(v, Font24), 2); }
  else text(col[1], 37, "--", Font24, kBlack);
  if (r.ens.state == SensorState::Ok) { formatUInt(v, sizeof v, r.ens.aqi, 1); text(col[2], 37, v, Font24, ink(aqiLevel(r.ens))); warnMark(aqiLevel(r.ens), col[2], 62, 17, 2); text(col[2] + 22, 49, aqiWord(r.ens.aqi), Font12, kBlack); }
  else text(col[2], 37, "--", Font24, kBlack);
  hLine(0, 295, 68);
  if (r.env.state == SensorState::Ok) {
    char n[12]; tempText(6, 74, Font16, r.env.temperatureC);
    formatFixed(n, sizeof n, r.env.humidityPct, 0); strcat(n, " %"); text(118, 74, n, Font16, kBlack);
    formatFixed(n, sizeof n, r.env.pressureHpa, 0); strcat(n, " hPa"); textRight(290, 74, n, Font16, kBlack);
  } else text(6, 74, "BME680 --", Font16, kBlack);
  hLine(0, 295, 96);
  char line[40];
  if (r.pm.state == SensorState::Ok) std::snprintf(line, sizeof line, "PM1 %u  PM10 %u", r.pm.pm1, r.pm.pm10); else std::snprintf(line, sizeof line, "PM1 --  PM10 --");
  text(6, 104, line, Font12, kBlack);
  if (r.ens.state == SensorState::Ok) std::snprintf(line, sizeof line, "TVOC %u ppb", r.ens.tvocPpb); else std::snprintf(line, sizeof line, "TVOC --");
  text(112, 104, line, Font12, kBlack);
  if (r.env.state == SensorState::Ok) { char n[12]; formatFixed(n, sizeof n, r.env.gasKOhm, 0); std::snprintf(line, sizeof line, "GAS %s kOhm", n); } else std::snprintf(line, sizeof line, "GAS --");
  textRight(290, 104, line, Font12, kBlack);
}

// ---------------------------------------------------------------- C: status board
void layoutStatusBoard(const Readings& r, uint32_t up, uint8_t* fb) {
  beginFrame(fb);
  header(r, up, false);
  hLine(0, 295, 19);
  const Level o = overallLevel(r);
  const char* s = levelName(o);
  uint8_t fill = kWhite, fg = kBlack;
  if (o == Level::Bad) { fill = kRed; fg = kWhite; fillRect(6, 26, 104, 70, kRed); }
  else if (o == Level::Warn) { outlineRect(6, 26, 104, 70, kRed); outlineRect(7, 27, 103, 69, kRed); }
  else outlineRect(6, 26, 104, 70, kBlack);
  textCentered(55, 38, s, Font20, fg, fill);
  const char* why = "all clear";
  if (o == Level::Unknown) why = "warming up";
  else if (co2Level(r.co2) == o && o != Level::Good) why = "cause: CO2";
  else if (pm25Level(r.pm) == o && o != Level::Good) why = "cause: PM2.5";
  else if (aqiLevel(r.ens) == o && o != Level::Good) why = "cause: VOC/AQI";
  textCentered(55, 78, why, Font12, kBlack);
  vLine(110, 22, 104);
  struct Row { const char* label; bool ok; char value[12]; const char* unit; Level level; };
  Row rows[4] = {
    {"CO2", r.co2.state == SensorState::Ok, "", "ppm", co2Level(r.co2)},
    {"PM2.5", r.pm.state == SensorState::Ok, "", "ug/m3", pm25Level(r.pm)},
    {"AQI", r.ens.state == SensorState::Ok, "", aqiWord(r.ens.aqi), aqiLevel(r.ens)},
    {"TVOC", r.ens.state == SensorState::Ok, "", "ppb", Level::Unknown},
  };
  formatUInt(rows[0].value, 12, r.co2.ppm, 1); formatUInt(rows[1].value, 12, r.pm.pm25, 1);
  formatUInt(rows[2].value, 12, r.ens.aqi, 1); formatUInt(rows[3].value, 12, r.ens.tvocPpb, 1);
  for (int i = 0; i < 4; ++i) {
    const int y = 24 + i * 20;
    text(118, y + 3, rows[i].label, Font12, kBlack);
    const char* v = rows[i].ok ? rows[i].value : "--";
    textRight(222, y, v, Font16, rows[i].ok ? ink(rows[i].level) : kBlack);
    if (rows[i].ok) { warnMark(rows[i].level, 222 - textWidth(v, Font16), y + 17, textWidth(v, Font16), 1); text(228, y + 3, rows[i].unit, Font12, kBlack); }
  }
  hLine(0, 295, 106);
  if (r.env.state == SensorState::Ok) {
    char n[12], l[40];
    tempText(6, 112, Font12, r.env.temperatureC);
    formatFixed(n, sizeof n, r.env.humidityPct, 0); std::snprintf(l, sizeof l, "%s %%", n); text(70, 112, l, Font12, kBlack);
    formatFixed(n, sizeof n, r.env.pressureHpa, 0); std::snprintf(l, sizeof l, "%s hPa", n); text(118, 112, l, Font12, kBlack);
    formatFixed(n, sizeof n, r.env.gasKOhm, 0); std::snprintf(l, sizeof l, "GAS %s kOhm", n); textRight(290, 112, l, Font12, kBlack);
  } else text(6, 112, "BME680 --", Font12, kBlack);
}

// ---------------------------------------------------------------- D: climate first
void layoutClimateFirst(const Readings& r, uint32_t up, uint8_t* fb) {
  beginFrame(fb);
  header(r, up, true);
  hLine(0, 295, 19);
  if (r.env.state == SensorState::Ok) {
    char n[12], l[24];
    tempText(6, 26, Font24, r.env.temperatureC);
    formatFixed(n, sizeof n, r.env.humidityPct, 0); std::snprintf(l, sizeof l, "%s %% rh", n); text(6, 56, l, Font16, kBlack);
    formatFixed(n, sizeof n, r.env.pressureHpa, 0); std::snprintf(l, sizeof l, "%s hPa", n); text(6, 78, l, Font12, kBlack);
    formatFixed(n, sizeof n, r.env.gasKOhm, 0); std::snprintf(l, sizeof l, "GAS %s kOhm", n); text(6, 92, l, Font12, kBlack);
  } else text(6, 26, "BME680 --", Font16, kBlack);
  vLine(126, 22, 100);
  struct Row { const char* label; bool ok; char value[12]; const char* unit; Level level; };
  Row rows[3] = {
    {"CO2", r.co2.state == SensorState::Ok, "", "ppm", co2Level(r.co2)},
    {"PM2.5", r.pm.state == SensorState::Ok, "", "ug/m3", pm25Level(r.pm)},
    {"AQI", r.ens.state == SensorState::Ok, "", aqiWord(r.ens.aqi), aqiLevel(r.ens)},
  };
  formatUInt(rows[0].value, 12, r.co2.ppm, 1); formatUInt(rows[1].value, 12, r.pm.pm25, 1); formatUInt(rows[2].value, 12, r.ens.aqi, 1);
  for (int i = 0; i < 3; ++i) {
    const int y = 24 + i * 26;
    text(134, y + 4, rows[i].label, Font12, kBlack);
    const char* v = rows[i].ok ? rows[i].value : "--";
    text(180, y, v, Font20, rows[i].ok ? ink(rows[i].level) : kBlack);
    if (rows[i].ok) { warnMark(rows[i].level, 180, y + 21, textWidth(v, Font20), 2); text(180 + textWidth(v, Font20) + 6, y + 6, rows[i].unit, Font12, kBlack); }
  }
  hLine(0, 295, 102);
  char line[40];
  if (r.pm.state == SensorState::Ok) std::snprintf(line, sizeof line, "PM1 %u  PM10 %u", r.pm.pm1, r.pm.pm10); else std::snprintf(line, sizeof line, "PM1 --  PM10 --");
  text(6, 110, line, Font12, kBlack);
  if (r.ens.state == SensorState::Ok) std::snprintf(line, sizeof line, "TVOC %u ppb", r.ens.tvocPpb); else std::snprintf(line, sizeof line, "TVOC --");
  text(134, 110, line, Font12, kBlack);
}

Readings good() {
  Readings r;
  r.co2 = {684, SensorState::Ok};
  r.pm = {5, 8, 11, SensorState::Ok};
  r.ens.aqi = 1; r.ens.tvocPpb = 42; r.ens.eco2Ppm = 612; r.ens.state = SensorState::Ok;
  r.env = {22.6f, 46.0f, 1012.3f, 86.2f, SensorState::Ok};
  return r;
}
Readings bad() {
  Readings r = good();
  r.co2.ppm = 1850; r.pm.pm25 = 48; r.pm.pm10 = 61; r.ens.aqi = 2; r.ens.tvocPpb = 1200;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "out";
  static uint8_t fb[kFrameBufferSize];
  struct L { const char* name; void (*fn)(const Readings&, uint32_t, uint8_t*); };
  const L layouts[] = {{"B_three_tiles", layoutThreeTiles}, {"C_status_board", layoutStatusBoard}, {"D_climate_first", layoutClimateFirst}};
  for (const L& l : layouts) {
    l.fn(good(), 3 * 3600 + 12 * 60, fb); writePpm(dir + "/layout_" + l.name + "_good.ppm", fb);
    l.fn(bad(), 5 * 86400 + 2 * 3600, fb); writePpm(dir + "/layout_" + l.name + "_bad.ppm", fb);
  }
  const Outdoor dry = {true, 9.8f, 2.9f, "p.cloudy", 0.0f, "14:32"};
  const Outdoor rain = {true, 6.0f, 9.0f, "rain", 4.2f, "17:05"};
  const Outdoor none = {false, 0, 0, "", 0, "--:--"};
  layoutOutdoorLine(good(), dry, fb); writePpm(dir + "/layout_W1_outdoor_line_good.ppm", fb);
  layoutOutdoorLine(bad(), rain, fb); writePpm(dir + "/layout_W1_outdoor_line_bad.ppm", fb);
  layoutOutdoorTile(good(), dry, fb); writePpm(dir + "/layout_W2_outdoor_tile_good.ppm", fb);
  layoutOutdoorTile(bad(), rain, fb); writePpm(dir + "/layout_W2_outdoor_tile_bad.ppm", fb);
  Readings boot; boot.env = {21.9f, 51.0f, 1008.7f, 12.0f, SensorState::Ok};
  layoutOutdoorTile(boot, none, fb); writePpm(dir + "/layout_W2_outdoor_tile_offline.ppm", fb);
  return 0;
}
