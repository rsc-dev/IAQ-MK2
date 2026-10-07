#include "forecast.h"

#include <string.h>

#include "src/ArduinoJson/ArduinoJson.h"

namespace {

bool startsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

// Advances the source until `token` has just been read. False at end of stream.
// The mismatch fallback restarts at the token's first character, which is
// exact only for tokens with no repeated prefix (true for "timeseries").
bool skipPast(ByteSource& src, const char* token) {
  const size_t len = strlen(token);
  size_t matched = 0;
  for (int c = src.read(); c >= 0; c = src.read()) {
    if ((char)c == token[matched]) {
      if (++matched == len) return true;
    } else {
      matched = ((char)c == token[0]) ? 1 : 0;
    }
  }
  return false;
}

}  // namespace

bool forecastExtractFirstEntry(ByteSource& body, char* out, size_t cap) {
  if (cap < 2) return false;
  if (!skipPast(body, "\"timeseries\"")) return false;
  int c = body.read();
  while (c >= 0 && c != '[') c = body.read();
  while (c >= 0 && c != '{') c = body.read();
  if (c < 0) return false;

  size_t n = 0;
  int depth = 0;
  bool inString = false;
  bool escape = false;
  for (; c >= 0; c = body.read()) {
    if (n + 1 >= cap) return false;
    out[n++] = (char)c;
    if (inString) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') ++depth;
    else if (c == '}' && --depth == 0) {
      out[n] = '\0';
      return true;
    }
  }
  return false;
}

bool forecastParseEntry(const char* json, OutdoorReading& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  JsonObjectConst instant = data["instant"]["details"].as<JsonObjectConst>();
  if (!instant["air_temperature"].is<float>() || !instant["wind_speed"].is<float>()) return false;

  out.tempC = instant["air_temperature"].as<float>();
  out.windMs = instant["wind_speed"].as<float>();

  const char* code = data["next_1_hours"]["summary"]["symbol_code"].as<const char*>();
  if (code == nullptr) code = data["next_6_hours"]["summary"]["symbol_code"].as<const char*>();
  strncpy(out.symbol, forecastSymbolWord(code ? code : ""), sizeof out.symbol - 1);
  out.symbol[sizeof out.symbol - 1] = '\0';

  JsonVariantConst rain = data["next_6_hours"]["details"]["precipitation_amount"];
  out.rainNext6hMm = rain.is<float>() ? rain.as<float>() : 0.0f;
  return true;
}

const char* forecastSymbolWord(const char* code) {
  if (strstr(code, "thunder")) return "storm";
  if (startsWith(code, "clearsky")) return "clear";
  if (startsWith(code, "fair")) return "fair";
  if (startsWith(code, "partlycloudy")) return "p.cloudy";
  if (startsWith(code, "cloudy")) return "cloudy";
  if (startsWith(code, "fog")) return "fog";
  if (strstr(code, "sleet")) return "sleet";
  if (startsWith(code, "heavysnow")) return "hvy snow";
  if (strstr(code, "snow")) return "snow";
  if (startsWith(code, "heavyrain")) return "hvy rain";
  if (startsWith(code, "lightrain")) return "lt.rain";
  if (startsWith(code, "rain")) return "rain";
  return "?";
}
