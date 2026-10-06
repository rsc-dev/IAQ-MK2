#pragma once
#include <stdint.h>
#include "readings.h"

// Pure classification of readings into traffic-light levels. Only readings in
// SensorState::Ok take part; everything else is Unknown.
enum class Level : uint8_t { Unknown = 0, Good, Warn, Bad };

Level co2Level(const Co2Reading& r);
Level pm25Level(const PmReading& r);
Level aqiLevel(const EnsReading& r);

// Worst level among the valid metrics; Unknown when none is valid.
Level overallLevel(const Readings& r);

const char* levelName(Level level);  // "WARMUP", "GOOD", "WARN", "BAD"
