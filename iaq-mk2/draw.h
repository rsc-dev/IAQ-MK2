#pragma once
#include <stdint.h>

#include "src/waveshare/fonts.h"

// Clipped drawing primitives on the 296 x 128 landscape frame (2 bits per
// pixel, Paint scale 4). They never rely on the vendor bounds checks, which are
// off by one, and never wrap text. Pure: no Arduino dependency.
namespace draw {

constexpr int kWidth = 296;
constexpr int kHeight = 128;
constexpr uint8_t kBlack = 0;
constexpr uint8_t kWhite = 1;
constexpr uint8_t kRed = 3;

// Binds the Paint library to `frameBuffer`, applies the configured rotation and clears to white.
void beginFrame(uint8_t* frameBuffer);

void setPixel(int x, int y, uint8_t colour);
void fillRect(int x0, int y0, int x1, int y1, uint8_t colour);
void hLine(int x0, int x1, int y, uint8_t colour = kBlack);
void vLine(int x, int y0, int y1, uint8_t colour = kBlack);
void outlineRect(int x0, int y0, int x1, int y1, uint8_t colour);

int textWidth(const char* s, const sFONT& font);
// Left-aligned text at (x, y); glyphs that would cross the right edge are dropped, non-ASCII renders as '?'.
void text(int x, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite);
void textRight(int xRight, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite);
void textCentered(int xCenter, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite);
// Small degree sign: a 3x3 ring with its top-left corner at (x, y).
void degree(int x, int y, uint8_t colour);

}  // namespace draw
