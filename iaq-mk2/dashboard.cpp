#include "dashboard.h"

#include <string.h>

#include "air_status.h"
#include "clock.h"
#include "config.h"
#include "draw.h"
#include "text_format.h"
#include "src/waveshare/EPD_2in9b_V4.h"
#include "src/waveshare/GUI_Paint.h"
#include "src/waveshare/fonts.h"

static_assert(kFrameBufferSize == ((EPD_2IN9B_V4_WIDTH + 3) / 4) * EPD_2IN9B_V4_HEIGHT, "framebuffer size");
static_assert(kPlaneSize == (EPD_2IN9B_V4_WIDTH / 8) * EPD_2IN9B_V4_HEIGHT, "plane size");

namespace {

constexpr uint8_t kBlack = kDashboardBlack;
constexpr uint8_t kWhite = kDashboardWhite;
constexpr uint8_t kRed = kDashboardRed;

using draw::fillRect;
using draw::hLine;
using draw::outlineRect;
using draw::setPixel;
using draw::textWidth;
using draw::vLine;
void drawText(int x, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite) { draw::text(x, y, s, font, fg, bg); }
void drawTextRight(int xRight, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite) { draw::textRight(xRight, y, s, font, fg, bg); }
void drawDegree(int x, int y, uint8_t colour) { draw::degree(x, y, colour); }

// ---- Layout (pixels) --------------------------------------------------------
constexpr int kMargin = 6;
constexpr int kHeaderTextY = 1;
constexpr int kRule1Y = 19;
constexpr int kLabelY = 23;
constexpr int kValueY = 37;          // Font24 row
constexpr int kDividerX = 147;
constexpr int kRightColumnX = 154;
constexpr int kPmSideX = 218;        // PM1 / PM10 column inside the PM2.5 tile
constexpr int kRule2Y = 66;
constexpr int kRowAY = 70;           // AQI word, TVOC
constexpr int kClimateY = 85;        // Font16 indoor climate
constexpr int kRule3Y = 104;
constexpr int kOutdoorY = 109;       // forecast line
constexpr float kRainWarnMm = 0.5f;

// The panel has black, white and red only: BAD is red, WARN is black with a red mark.
uint8_t levelColour(Level level) { return level == Level::Bad ? kRed : kBlack; }

void drawWarnUnderline(int x, int y, int width, int thickness) {
  fillRect(x, y, x + width - 1, y + thickness - 1, kRed);
}

// Short hint shown next to "--" when a value is not valid.
const char* stateHint(SensorState s) {
  switch (s) {
    case SensorState::NotReady: return "init";
    case SensorState::WarmingUp: return "warm-up";
    case SensorState::Stale: return "no data";
    case SensorState::Unavailable: return "n/a";
    case SensorState::Ok: return "";
  }
  return "";
}

const char* aqiWord(uint8_t aqi) {
  switch (aqi) {
    case 1: return "excellent";
    case 2: return "good";
    case 3: return "moderate";
    case 4: return "poor";
    case 5: return "unhealthy";
  }
  return "?";
}

// Temperature with a degree ring and "C", returns the x after the text.
int drawTemperature(int x, int y, const sFONT& font, float celsius, uint8_t colour) {
  char n[12];
  formatFixed(n, sizeof n, celsius, font.Height >= 16 ? 1 : 0);
  drawText(x, y, n, font, colour);
  x += textWidth(n, font) + 3;
  drawDegree(x, y + 2, colour);
  drawText(x + 5, y, "C", font, colour);
  return x + 5 + (int)font.Width;
}

// ---- Sections ----------------------------------------------------------------
void drawHeader(const Readings& r) {
  drawText(kMargin - 2, kHeaderTextY, "AIR QUALITY", Font16, kBlack);

  const Level overall = overallLevel(r);
  const char* status = levelName(overall);
  const int badgeRight = kDashboardWidth - 4;
  const int badgeLeft = badgeRight - textWidth(status, Font16) - 9;
  const int badgeTop = 0;
  const int badgeBottom = 17;

  uint8_t fill = kWhite;
  uint8_t ink = kBlack;
  if (overall == Level::Bad) {
    fill = kRed;
    ink = kWhite;
    fillRect(badgeLeft, badgeTop, badgeRight, badgeBottom, fill);
  } else if (overall == Level::Warn) {
    outlineRect(badgeLeft, badgeTop, badgeRight, badgeBottom, kRed);
    outlineRect(badgeLeft + 1, badgeTop + 1, badgeRight - 1, badgeBottom - 1, kRed);
  } else if (overall == Level::Good) {
    outlineRect(badgeLeft, badgeTop, badgeRight, badgeBottom, kBlack);
  }
  drawText(badgeLeft + 5, kHeaderTextY, status, Font16, ink, fill);

  char clock[8];
  formatClockWarsaw(clock, sizeof clock, r.time.valid ? r.time.epoch : 0u);
  drawTextRight(badgeLeft - 8, kHeaderTextY, clock, Font16, kBlack);
}

// Big value tile: value alone in Font24 (units live in the label), red when BAD,
// red underline when WARN, "--" plus a hint otherwise.
void drawTileValue(int x, const char* value, Level level, bool valid, SensorState state) {
  if (valid) {
    drawText(x, kValueY, value, Font24, levelColour(level));
    if (level == Level::Warn) drawWarnUnderline(x, kValueY + (int)Font24.Height + 1, textWidth(value, Font24), 2);
  } else {
    drawText(x, kValueY, "--", Font24, kBlack);
    drawText(x + textWidth("--", Font24) + 5, kValueY + (int)Font24.Height - (int)Font12.Height, stateHint(state), Font12, kBlack);
  }
}

void drawMainTiles(const Readings& r) {
  char value[12];
  char line[16];

  drawText(kMargin, kLabelY, "CO2 ppm", Font12, kBlack);
  formatUInt(value, sizeof value, r.co2.ppm, 1);
  drawTileValue(kMargin, value, co2Level(r.co2), r.co2.state == SensorState::Ok, r.co2.state);

  drawText(kRightColumnX, kLabelY, "PM2.5 ug/m3", Font12, kBlack);
  formatUInt(value, sizeof value, r.pm.pm25, 1);
  drawTileValue(kRightColumnX, value, pm25Level(r.pm), r.pm.state == SensorState::Ok, r.pm.state);
  if (r.pm.state == SensorState::Ok) {
    strcpy(line, "PM1 ");
    formatUInt(value, sizeof value, r.pm.pm1, 1);
    strcat(line, value);
    drawText(kPmSideX, kValueY + 1, line, Font12, kBlack);
    strcpy(line, "PM10 ");
    formatUInt(value, sizeof value, r.pm.pm10, 1);
    strcat(line, value);
    drawText(kPmSideX, kValueY + 15, line, Font12, kBlack);
  }

  vLine(kDividerX, kRule1Y + 2, kRule2Y - 2);
}

void drawAirRow(const EnsReading& ens) {
  char num[12];
  char line[24];
  if (ens.state != SensorState::Ok) {
    drawText(kMargin, kRowAY, "AQI -  TVOC --", Font12, kBlack);
    return;
  }
  drawText(kMargin, kRowAY, "AQI ", Font12, kBlack);
  const Level aqi = aqiLevel(ens);
  const int aqiX = kMargin + textWidth("AQI ", Font12);
  formatUInt(num, sizeof num, ens.aqi, 1);
  drawText(aqiX, kRowAY, num, Font12, levelColour(aqi));
  if (aqi == Level::Warn) drawWarnUnderline(aqiX, kRowAY + (int)Font12.Height, textWidth(num, Font12), 1);
  drawText(aqiX + textWidth(num, Font12) + 7, kRowAY, aqiWord(ens.aqi), Font12, kBlack);

  strcpy(line, "TVOC ");
  formatUInt(num, sizeof num, ens.tvocPpb, 1);
  strcat(line, num);
  strcat(line, " ppb");
  drawTextRight(kDashboardWidth - kMargin, kRowAY, line, Font12, kBlack);
}

void drawClimate(const EnvReading& env) {
  if (env.state != SensorState::Ok) {
    drawText(kMargin, kClimateY, env.state == SensorState::Unavailable ? "BME680 not found" : "BME680 --", Font16, kBlack);
    return;
  }
  char num[12];
  char text[16];
  drawTemperature(kMargin, kClimateY, Font16, env.temperatureC, kBlack);
  formatFixed(num, sizeof num, env.humidityPct, 0);
  strcpy(text, num);
  strcat(text, " %");
  drawText(118, kClimateY, text, Font16, kBlack);
  formatFixed(num, sizeof num, env.pressureHpa, 0);
  strcpy(text, num);
  strcat(text, " hPa");
  drawTextRight(kDashboardWidth - kMargin, kClimateY, text, Font16, kBlack);
}

void drawOutdoor(const OutdoorReading& o) {
  if (o.state != SensorState::Ok) {
    drawText(kMargin, kOutdoorY, "Gdynia: no forecast", Font12, kBlack);
    return;
  }
  char num[12];
  char text[24];
  drawText(kMargin, kOutdoorY, "Gdynia", Font12, kBlack);
  int x = drawTemperature(kMargin + textWidth("Gdynia ", Font12), kOutdoorY, Font12, o.tempC, kBlack) + 10;
  strcpy(text, o.symbol);
  drawText(x, kOutdoorY, text, Font12, kBlack);
  x += textWidth(text, Font12) + 10;
  formatFixed(num, sizeof num, o.windMs, 0);
  strcpy(text, num);
  strcat(text, " m/s");
  drawText(x, kOutdoorY, text, Font12, kBlack);
  x += textWidth(text, Font12) + 10;
  formatFixed(num, sizeof num, o.rainNext6hMm, 1);
  strcpy(text, "6h ");
  strcat(text, num);
  strcat(text, " mm");
  drawText(x, kOutdoorY, text, Font12, o.rainNext6hMm >= kRainWarnMm ? kRed : kBlack);
}

}  // namespace

void dashboardRender(const Readings& r, uint8_t* frameBuffer) {
  draw::beginFrame(frameBuffer);
  drawHeader(r);
  hLine(0, kDashboardWidth - 1, kRule1Y);
  drawMainTiles(r);
  hLine(0, kDashboardWidth - 1, kRule2Y);
  drawAirRow(r.ens);
  drawClimate(r.env);
  hLine(0, kDashboardWidth - 1, kRule3Y);
  drawOutdoor(r.outdoor);
}

namespace {
// Landscape (x, y) -> panel memory (column, row); inverse of Paint_SetPixel for rotation 90 / 270.
void toMemory(int x, int y, int& mx, int& my) {
  if (cfg::DISPLAY_ROTATION == 90) {
    mx = EPD_2IN9B_V4_WIDTH - y - 1;
    my = x;
  } else {
    mx = y;
    my = EPD_2IN9B_V4_HEIGHT - x - 1;
  }
}

uint8_t colourAt(const uint8_t* frameBuffer, int mx, int my) {
  const uint8_t byte = frameBuffer[mx / 4 + my * (EPD_2IN9B_V4_WIDTH / 4)];
  return (uint8_t)((byte >> (6 - 2 * (mx % 4))) & 0x3);
}
}  // namespace

uint8_t dashboardPixel(const uint8_t* frameBuffer, int x, int y) {
  if (x < 0 || y < 0 || x >= kDashboardWidth || y >= kDashboardHeight) return kWhite;
  int mx, my;
  toMemory(x, y, mx, my);
  return colourAt(frameBuffer, mx, my);
}

void dashboardSplitPlanes(const uint8_t* frameBuffer, uint8_t* blackPlane, uint8_t* redPlane) {
  constexpr int kBytesPerRow = EPD_2IN9B_V4_WIDTH / 8;
  for (int my = 0; my < EPD_2IN9B_V4_HEIGHT; ++my) {
    for (int b = 0; b < kBytesPerRow; ++b) {
      uint8_t black = 0xFF;
      uint8_t red = 0xFF;
      for (int bit = 0; bit < 8; ++bit) {
        const uint8_t colour = colourAt(frameBuffer, b * 8 + bit, my);
        const uint8_t mask = (uint8_t)(0x80 >> bit);
        if (colour == kBlack) black &= (uint8_t)~mask;
        if (colour == kRed) red &= (uint8_t)~mask;
      }
      blackPlane[b + my * kBytesPerRow] = black;
      redPlane[b + my * kBytesPerRow] = red;
    }
  }
}

uint8_t dashboardPlaneBit(const uint8_t* plane, int x, int y) {
  if (x < 0 || y < 0 || x >= kDashboardWidth || y >= kDashboardHeight) return 1;
  int mx, my;
  toMemory(x, y, mx, my);
  return (uint8_t)((plane[mx / 8 + my * (EPD_2IN9B_V4_WIDTH / 8)] >> (7 - (mx % 8))) & 0x1);
}
