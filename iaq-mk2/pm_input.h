#pragma once
#include <stdint.h>
#include "readings.h"

// PMS7003 on Serial1 (D0 RX / D1 TX), active mode, 9600 baud.
namespace pm_input {
void begin();
// Drains the UART into the parser and applies warm-up / stale rules.
void update(uint32_t nowMs, PmReading& out);
uint32_t framesOk();
uint32_t checksumErrors();
}  // namespace pm_input
