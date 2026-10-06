#pragma once
#include <stdint.h>
#include "readings.h"

// BME680 (temperature, humidity, pressure, gas) and ENS160 (AQI, TVOC, eCO2)
// on the I2C bus. BME680 is read asynchronously; its result feeds the ENS160
// temperature/humidity compensation before the ENS160 is read.
namespace env_sensors {
void begin(EnvReading& env, EnsReading& ens);
void update(uint32_t nowMs, EnvReading& env, EnsReading& ens);
bool bmePresent();
bool ensPresent();
}  // namespace env_sensors
