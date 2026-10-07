#pragma once
#include <stdint.h>
#include "readings.h"

// Hourly forecast fetch from MET Norway over HTTPS. The blocking parts (TLS
// handshake, download) run in a worker thread so loop() and the watchdog keep
// going; update() only schedules fetches and copies finished results.
namespace weather {
void begin();
void update(uint32_t nowMs, OutdoorReading& out);
uint32_t fetchesOk();
uint32_t fetchesFailed();
const char* lastError();   // "" when the last fetch succeeded
}  // namespace weather
