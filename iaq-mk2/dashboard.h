#pragma once
#include <stddef.h>
#include <stdint.h>
#include "readings.h"

// Renders the status panel into a 2-bit-per-pixel framebuffer (128 x 296 panel
// memory, shown as 296 x 128 landscape) for the Waveshare 2.9" (B) V4 panel.
// Pure: depends only on the Paint library, no Arduino APIs.

constexpr int kDashboardWidth = 296;
constexpr int kDashboardHeight = 128;
constexpr size_t kFrameBufferSize = (128 / 4) * 296;  // 9472 bytes, 2 bits per pixel
constexpr size_t kPlaneSize = (128 / 8) * 296;        // 4736 bytes, 1 bit per pixel

// Colour codes in the 2-bpp frame. The 2.9" (B) V4 panel has no yellow; WARN is
// drawn as black with red marks, BAD in red.
constexpr uint8_t kDashboardBlack = 0;
constexpr uint8_t kDashboardWhite = 1;
constexpr uint8_t kDashboardRed = 3;

void dashboardRender(const Readings& r, uint8_t* frameBuffer);

// Colour code (0 black, 1 white, 3 red) of landscape pixel (x, y) in a buffer
// produced by dashboardRender. For tests and previews.
uint8_t dashboardPixel(const uint8_t* frameBuffer, int x, int y);

// Splits the 2-bpp frame into the two 1-bpp planes the SSD1680 panel takes, in
// the Waveshare image convention the vendor driver expects: black plane bit 0 =
// black ink, red plane bit 0 = red ink, bit 1 = nothing. Same memory layout
// (128 columns x 296 rows, MSB first).
void dashboardSplitPlanes(const uint8_t* frameBuffer, uint8_t* blackPlane, uint8_t* redPlane);

// Bit (0 or 1) of landscape pixel (x, y) in a 1-bpp plane. For tests.
uint8_t dashboardPlaneBit(const uint8_t* plane, int x, int y);
