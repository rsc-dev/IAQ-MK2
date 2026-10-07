#include "dashboard.h"

#include <string.h>

#include "air_status.h"
#include "config.h"
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

  char up[20];
  char text[28] = "up ";
  formatUptime(up, sizeof up, uptimeSeconds);
  strncat(text, up, sizeof text - strlen(text) - 1);
  drawTextRight(badgeLeft - 8, kHeaderTextY + 3, text, Font12, kBlack);
}

// Big value tile: label, Font24 value (red when BAD, red underline when WARN), Font12 unit.
void drawTile(int x, const char* label, const char* value, const char* unit, Level level,
              bool valid, SensorState state) {
  drawText(x, kLabelY, label, Font12, kBlack);
  if (valid) {
    drawText(x, kValueY, value, Font24, levelColour(level));
    if (level == Level::Warn) drawWarnUnderline(x, kValueY + (int)Font24.Height + 1, textWidth(value, Font24), 2);
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
  drawTile(kMargin, "CO2", value, "ppm", co2Level(r.co2), r.co2.state == SensorState::Ok, r.co2.state);

  formatUInt(value, sizeof value, r.pm.pm25, 1);
  drawTile(kRightColumnX, "PM2.5", value, "ug/m3", pm25Level(r.pm), r.pm.state == SensorState::Ok, r.pm.state);

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
    const Level aqi = aqiLevel(r.ens);
    const int aqiX = kRightColumnX + textWidth("AQI ", Font12);
    drawText(aqiX, kRowAY, num, Font12, levelColour(aqi));
    if (aqi == Level::Warn) drawWarnUnderline(aqiX, kRowAY + (int)Font12.Height, textWidth(num, Font12), 1);
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
  Paint_NewImage(frameBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, cfg::DISPLAY_ROTATION, kWhite);
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
