#pragma once
#include <stddef.h>
#include "byte_source.h"
#include "readings.h"

// MET Norway Locationforecast 2.0 (compact) handling. The reply is ~37 KB;
// only the first `timeseries` entry (the current hour) is needed, so it is
// copied out of the stream (about 600 bytes) and parsed on its own.
// Pure; ArduinoJson is used for the small entry only.

// Copies the first object of "timeseries": [ ... ] into `out` (NUL-terminated).
// False if the array is not found, the stream ends early, or `cap` is too small.
bool forecastExtractFirstEntry(ByteSource& body, char* out, size_t cap);

// Parses one timeseries entry. Requires air_temperature and wind_speed;
// symbol and 6-hour precipitation default to "?" and 0. Leaves `state`,
// `fetchedEpoch` untouched.
bool forecastParseEntry(const char* json, OutdoorReading& out);

// Short ASCII word for a met.no symbol code, e.g. "partlycloudy_day" -> "p.cloudy".
const char* forecastSymbolWord(const char* code);
