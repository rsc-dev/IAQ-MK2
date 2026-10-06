#pragma once
#include <stdint.h>

// RP2040 hardware watchdog (mbed driver). A hang anywhere reboots the board.
namespace watchdog {
void begin(uint32_t timeoutMs);  // clamped to the hardware maximum (~8.3 s)
void kick();
bool active();
}  // namespace watchdog
