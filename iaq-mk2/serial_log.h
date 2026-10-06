#pragma once
#include <stdint.h>
#include "readings.h"

// USB serial logging. One key=value summary line per LOG_INTERVAL_MS plus
// tagged event lines. Never blocks when no terminal is attached.
namespace serial_log {
void begin();
// Handles a pending 1200-baud upload request (enters the bootloader). Call from loop().
void update();
void event(const char* tag, const char* message);
void eventValue(const char* tag, const char* message, uint32_t value);
void summary(const Readings& r, uint32_t uptimeSeconds);
}  // namespace serial_log
