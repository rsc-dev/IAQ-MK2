#include "draw.h"

#include <string.h>

#include "config.h"
#include "src/waveshare/EPD_2in9b_V4.h"
#include "src/waveshare/GUI_Paint.h"

namespace draw {

void beginFrame(uint8_t* frameBuffer) {
  Paint_NewImage(frameBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, cfg::DISPLAY_ROTATION, kWhite);
  Paint_SetScale(4);
  Paint_SelectImage(frameBuffer);
  Paint_Clear(kWhite);
}

void setPixel(int x, int y, uint8_t colour) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
  Paint_SetPixel((UWORD)x, (UWORD)y, colour);
}

void fillRect(int x0, int y0, int x1, int y1, uint8_t colour) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) setPixel(x, y, colour);
}

void hLine(int x0, int x1, int y, uint8_t colour) { fillRect(x0, y, x1, y, colour); }
void vLine(int x, int y0, int y1, uint8_t colour) { fillRect(x, y0, x, y1, colour); }

void outlineRect(int x0, int y0, int x1, int y1, uint8_t colour) {
  hLine(x0, x1, y0, colour);
  hLine(x0, x1, y1, colour);
  vLine(x0, y0, y1, colour);
  vLine(x1, y0, y1, colour);
}

int textWidth(const char* s, const sFONT& font) { return (int)strlen(s) * (int)font.Width; }

void text(int x, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg) {
  if (y < 0 || y + (int)font.Height > kHeight) return;
  for (; *s != '\0'; ++s, x += (int)font.Width) {
    if (x < 0) continue;
    if (x + (int)font.Width > kWidth) return;
    char c = *s;
    if (c < ' ' || c > '~') c = '?';
    // Paint_DrawChar has the documented (foreground, background) order.
    Paint_DrawChar((UWORD)x, (UWORD)y, c, const_cast<sFONT*>(&font), fg, bg);
  }
}

void textRight(int xRight, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg) {
  text(xRight - textWidth(s, font), y, s, font, fg, bg);
}

void textCentered(int xCenter, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg) {
  text(xCenter - textWidth(s, font) / 2, y, s, font, fg, bg);
}

void degree(int x, int y, uint8_t colour) {
  setPixel(x + 1, y, colour);
  setPixel(x, y + 1, colour);
  setPixel(x + 2, y + 1, colour);
  setPixel(x + 1, y + 2, colour);
}

}  // namespace draw
