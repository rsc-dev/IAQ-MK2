#pragma once
#include <stddef.h>
#include <stdint.h>
#include "readings.h"

// Renders the status panel into a 2-bit-per-pixel framebuffer for the
// Waveshare 2.9" (G) panel (128 x 296 memory, shown as 296 x 128 landscape).
// Pure: depends only on the Paint library, no Arduino APIs.

constexpr int kDashboardWidth = 296;
constexpr int kDashboardHeight = 128;
constexpr size_t kFrameBufferSize = (128 / 4) * 296;  // 9472 bytes

void dashboardRender(const Readings& r, uint32_t uptimeSeconds, uint8_t* frameBuffer);

// Colour code (0 black, 1 white, 2 yellow, 3 red) of landscape pixel (x, y)
// in a buffer produced by dashboardRender. For tests and previews.
uint8_t dashboardPixel(const uint8_t* frameBuffer, int x, int y);
