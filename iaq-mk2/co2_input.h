#pragma once
#include <stdint.h>
#include "readings.h"

// SEN0219 PWM capture on cfg::PIN_CO2_PWM using a CHANGE interrupt.
namespace co2_input {
void begin();
// Converts the latest complete PWM cycle and applies warm-up / stale rules.
void update(uint32_t nowMs, Co2Reading& out);
uint32_t cyclesSeen();
uint32_t cyclesRejected();
}  // namespace co2_input
