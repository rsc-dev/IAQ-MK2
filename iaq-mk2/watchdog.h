#pragma once
#include <stdint.h>

// RP2040 hardware watchdog (mbed driver). A hang anywhere reboots the board.
namespace watchdog {
void begin(uint32_t timeoutMs);  // clamped to the hardware maximum (~8.3 s)
void kick();
bool active();
// Stops kicking for good so a pending bootrom reboot (reset_usb_boot) can fire.
void suspend();
}  // namespace watchdog
