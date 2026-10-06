#include "dashboard.h"

#include <string.h>

#include "air_status.h"
#include "config.h"
#include "text_format.h"
#include "src/waveshare/EPD_2in9g.h"
#include "src/waveshare/GUI_Paint.h"
#include "src/waveshare/fonts.h"

static_assert(kFrameBufferSize == ((EPD_2IN9G_WIDTH + 3) / 4) * EPD_2IN9G_HEIGHT, "framebuffer size");

namespace {

constexpr uint8_t kBlack = EPD_2IN9G_BLACK;
constexpr uint8_t kWhite = EPD_2IN9G_WHITE;
constexpr uint8_t kYellow = EPD_2IN9G_YELLOW;
constexpr uint8_t kRed = EPD_2IN9G_RED;

// ---- Layout (pixels) --------------------------------------------------------
constexpr int kMargin = 6;
constexpr int kHeaderTextY = 1;
constexpr int kRule1Y = 19;
constexpr int kLabelY = 23;
constexpr int kValueY = 37;          // Font24 baseline row
constexpr int kDividerX = 147;
constexpr int kRule2Y = 66;
constexpr int kRowAY = 70;
constexpr int kRowBY = 84;
constexpr int kRule3Y = 98;
constexpr int kBottomY = 104;
constexpr int kRightColumnX = 154;

// ---- Primitive drawing (clipped; never relies on vendor bounds checks) -------
void setPixel(int x, int y, uint8_t colour) {
  if (x < 0 || y < 0 || x >= kDashboardWidth || y >= kDashboardHeight) return;
  Paint_SetPixel((UWORD)x, (UWORD)y, colour);
}

void fillRect(int x0, int y0, int x1, int y1, uint8_t colour) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) setPixel(x, y, colour);
}

void hLine(int x0, int x1, int y, uint8_t colour = kBlack) { fillRect(x0, y, x1, y, colour); }
void vLine(int x, int y0, int y1, uint8_t colour = kBlack) { fillRect(x, y0, x, y1, colour); }

void outlineRect(int x0, int y0, int x1, int y1, uint8_t colour) {
  hLine(x0, x1, y0, colour);
  hLine(x0, x1, y1, colour);
  vLine(x0, y0, y1, colour);
  vLine(x1, y0, y1, colour);
}

int textWidth(const char* s, const sFONT& font) { return (int)strlen(s) * (int)font.Width; }

// Draws text left-aligned at (x, y) without wrapping. Glyphs that would cross
// the right edge are dropped. Non-ASCII characters render as '?'.
void drawText(int x, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite) {
  if (y < 0 || y + (int)font.Height > kDashboardHeight) return;
  for (; *s != '\0'; ++s, x += (int)font.Width) {
    if (x < 0) continue;
    if (x + (int)font.Width > kDashboardWidth) return;
    char c = *s;
    if (c < ' ' || c > '~') c = '?';
    // Paint_DrawChar has the documented (foreground, background) order.
    Paint_DrawChar((UWORD)x, (UWORD)y, c, const_cast<sFONT*>(&font), fg, bg);
  }
}

void drawTextRight(int xRight, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite) {
  drawText(xRight - textWidth(s, font), y, s, font, fg, bg);
}

// Small degree sign: a 3x3 ring with its top-left corner at (x, y).
void drawDegree(int x, int y, uint8_t colour) {
  setPixel(x + 1, y, colour);
  setPixel(x, y + 1, colour);
  setPixel(x + 2, y + 1, colour);
  setPixel(x + 1, y + 2, colour);
}

uint8_t levelColour(Level level) {
  switch (level) {
    case Level::Bad: return kRed;
    case Level::Warn: return kYellow;
    default: return kBlack;
  }
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

// ---- Sections ----------------------------------------------------------------
void drawHeader(const Readings& r, uint32_t uptimeSeconds) {
  drawText(kMargin - 2, kHeaderTextY, "AIR QUALITY", Font16, kBlack);

  const Level overall = overallLevel(r);
  const char* status = levelName(overall);
  const int badgeTextW = textWidth(status, Font16);
  const int badgeRight = kDashboardWidth - 4;
  const int badgeLeft = badgeRight - badgeTextW - 9;
  const int badgeTop = 0;
  const int badgeBottom = 17;

  uint8_t fill = kWhite;
  uint8_t ink = kBlack;
  if (overall == Level::Bad) { fill = kRed; ink = kWhite; }
  if (overall == Level::Warn) { fill = kYellow; ink = kBlack; }
  if (fill != kWhite) {
    fillRect(badgeLeft, badgeTop, badgeRight, badgeBottom, fill);
  } else if (overall == Level::Good) {
    outlineRect(badgeLeft, badgeTop, badgeRight, badgeBottom, kBlack);
  }
  drawText(badgeLeft + 5, kHeaderTextY, status, Font16, ink, fill);

  char up[20];
  char text[28] = "up ";
  formatUptime(up, sizeof up, uptimeSeconds);
  strncat(text, up, sizeof text - strlen(text) - 1);
  drawTextRight(badgeLeft - 8, kHeaderTextY + 3, text, Font12, kBlack);
}

// Big value tile: label, Font24 value in the level colour, Font12 unit.
void drawTile(int x, const char* label, const char* value, const char* unit, uint8_t valueColour,
              bool valid, SensorState state) {
  drawText(x, kLabelY, label, Font12, kBlack);
  if (valid) {
    drawText(x, kValueY, value, Font24, valueColour);
    const int unitX = x + textWidth(value, Font24) + 5;
    drawText(unitX, kValueY + (int)Font24.Height - (int)Font12.Height, unit, Font12, kBlack);
  } else {
    drawText(x, kValueY, "--", Font24, kBlack);
    const int hintX = x + textWidth("--", Font24) + 5;
    drawText(hintX, kValueY + (int)Font24.Height - (int)Font12.Height, stateHint(state), Font12, kBlack);
  }
}

void drawMainTiles(const Readings& r) {
  char value[12];

  formatUInt(value, sizeof value, r.co2.ppm, 1);
  drawTile(kMargin, "CO2", value, "ppm", levelColour(co2Level(r.co2)), r.co2.state == SensorState::Ok, r.co2.state);

  formatUInt(value, sizeof value, r.pm.pm25, 1);
  drawTile(kRightColumnX, "PM2.5", value, "ug/m3", levelColour(pm25Level(r.pm)), r.pm.state == SensorState::Ok,
           r.pm.state);

  vLine(kDividerX, kRule1Y + 2, kRule2Y - 2);
}

void drawSecondaryRows(const Readings& r) {
  char line[40];
  char num[12];

  // Row A left: PM1 / PM10
  if (r.pm.state == SensorState::Ok) {
    char pm1[8], pm10[8];
    formatUInt(pm1, sizeof pm1, r.pm.pm1, 1);
    formatUInt(pm10, sizeof pm10, r.pm.pm10, 1);
    strcpy(line, "PM1 ");
    strcat(line, pm1);
    strcat(line, "  PM10 ");
    strcat(line, pm10);
  } else {
    strcpy(line, "PM1 --  PM10 --");
  }
  drawText(kMargin, kRowAY, line, Font12, kBlack);

  // Row A right: AQI (coloured) and TVOC
  const bool ensOk = r.ens.state == SensorState::Ok;
  drawText(kRightColumnX, kRowAY, "AQI ", Font12, kBlack);
  if (ensOk) {
    formatUInt(num, sizeof num, r.ens.aqi, 1);
    drawText(kRightColumnX + textWidth("AQI ", Font12), kRowAY, num, Font12, levelColour(aqiLevel(r.ens)));
    formatUInt(num, sizeof num, r.ens.tvocPpb, 1);
    strcpy(line, "TVOC ");
    strcat(line, num);
    strcat(line, " ppb");
  } else {
    drawText(kRightColumnX + textWidth("AQI ", Font12), kRowAY, "-", Font12, kBlack);
    strcpy(line, "TVOC --");
  }
  drawText(kRightColumnX + 42, kRowAY, line, Font12, kBlack);

  // Row B left: BME680 gas resistance
  if (r.env.state == SensorState::Ok) {
    formatFixed(num, sizeof num, r.env.gasKOhm, 0);
    strcpy(line, "GAS ");
    strcat(line, num);
    strcat(line, " kOhm");
  } else {
    strcpy(line, "GAS --");
  }
  drawText(kMargin, kRowBY, line, Font12, kBlack);

  // Row B right: ENS160 health hint when it is not delivering valid data
  if (!ensOk) {
    strcpy(line, "ENS160 ");
    strcat(line, r.ens.state == SensorState::WarmingUp && r.ens.validityFlag == 2 ? "start-up" : stateHint(r.ens.state));
    drawText(kRightColumnX, kRowBY, line, Font12, kBlack);
  }
}

void drawEnvironment(const EnvReading& env) {
  char num[12];
  if (env.state != SensorState::Ok) {
    const char* msg = env.state == SensorState::Unavailable ? "BME680 not found" : "BME680 --";
    drawText(kMargin, kBottomY, msg, Font16, kBlack);
    return;
  }
  // Temperature with a degree ring, e.g. "22.6 oC"
  formatFixed(num, sizeof num, env.temperatureC, 1);
  drawText(kMargin, kBottomY, num, Font16, kBlack);
  int x = kMargin + textWidth(num, Font16) + 3;
  drawDegree(x, kBottomY + 2, kBlack);
  drawText(x + 5, kBottomY, "C", Font16, kBlack);

  formatFixed(num, sizeof num, env.humidityPct, 0);
  char rh[12];
  strcpy(rh, num);
  strcat(rh, " %");
  drawText(118, kBottomY, rh, Font16, kBlack);

  formatFixed(num, sizeof num, env.pressureHpa, 0);
  char p[16];
  strcpy(p, num);
  strcat(p, " hPa");
  drawTextRight(kDashboardWidth - kMargin, kBottomY, p, Font16, kBlack);
}

}  // namespace

void dashboardRender(const Readings& r, uint32_t uptimeSeconds, uint8_t* frameBuffer) {
  Paint_NewImage(frameBuffer, EPD_2IN9G_WIDTH, EPD_2IN9G_HEIGHT, cfg::DISPLAY_ROTATION, kWhite);
  Paint_SetScale(4);
  Paint_SelectImage(frameBuffer);
  Paint_Clear(kWhite);

  drawHeader(r, uptimeSeconds);
  hLine(0, kDashboardWidth - 1, kRule1Y);
  drawMainTiles(r);
  hLine(0, kDashboardWidth - 1, kRule2Y);
  drawSecondaryRows(r);
  hLine(0, kDashboardWidth - 1, kRule3Y);
  drawEnvironment(r.env);
}

uint8_t dashboardPixel(const uint8_t* frameBuffer, int x, int y) {
  if (x < 0 || y < 0 || x >= kDashboardWidth || y >= kDashboardHeight) return kWhite;
  // Inverse of Paint_SetPixel for rotation 90 / 270 with WidthMemory = 128.
  int mx, my;
  if (cfg::DISPLAY_ROTATION == 90) {
    mx = EPD_2IN9G_WIDTH - y - 1;
    my = x;
  } else {
    mx = y;
    my = EPD_2IN9G_HEIGHT - x - 1;
  }
  const uint8_t byte = frameBuffer[mx / 4 + my * (EPD_2IN9G_WIDTH / 4)];
  return (uint8_t)((byte >> (6 - 2 * (mx % 4))) & 0x3);
}
