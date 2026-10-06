#include "serial_log.h"

#include <Arduino.h>

#include "air_status.h"
#include "co2_input.h"
#include "config.h"
#include "pm_input.h"
#include "text_format.h"

namespace {
void kv(const char* key, const char* value) {
  Serial.print(' ');
  Serial.print(key);
  Serial.print('=');
  Serial.print(value);
}
void kvUInt(const char* key, uint32_t value) {
  Serial.print(' ');
  Serial.print(key);
  Serial.print('=');
  Serial.print(value);
}
void kvFixed(const char* key, float value, uint8_t decimals) {
  char b[16];
  formatFixed(b, sizeof b, value, decimals);
  kv(key, b);
}
}  // namespace

void serial_log::begin() { Serial.begin(cfg::SERIAL_BAUD); }

void serial_log::event(const char* tag, const char* message) {
  Serial.print('[');
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

void serial_log::eventValue(const char* tag, const char* message, uint32_t value) {
  Serial.print('[');
  Serial.print(tag);
  Serial.print("] ");
  Serial.print(message);
  Serial.println(value);
}

void serial_log::summary(const Readings& r, uint32_t uptimeSeconds) {
  Serial.print("t=");
  Serial.print(uptimeSeconds);
  kv("status", levelName(overallLevel(r)));

  kvUInt("co2", r.co2.ppm);
  kv("co2_state", sensorStateName(r.co2.state));
  kvUInt("co2_cycles", co2_input::cyclesSeen());
  kvUInt("co2_rejected", co2_input::cyclesRejected());

  kvUInt("pm1", r.pm.pm1);
  kvUInt("pm25", r.pm.pm25);
  kvUInt("pm10", r.pm.pm10);
  kv("pm_state", sensorStateName(r.pm.state));
  kvUInt("pm_frames", pm_input::framesOk());
  kvUInt("pm_crc_err", pm_input::checksumErrors());

  kvUInt("aqi", r.ens.aqi);
  kvUInt("tvoc", r.ens.tvocPpb);
  kvUInt("eco2", r.ens.eco2Ppm);
  kvUInt("ens_validity", r.ens.validityFlag);
  kv("ens_state", sensorStateName(r.ens.state));

  kvFixed("temp", r.env.temperatureC, 1);
  kvFixed("rh", r.env.humidityPct, 1);
  kvFixed("press", r.env.pressureHpa, 1);
  kvFixed("gas", r.env.gasKOhm, 1);
  kv("env_state", sensorStateName(r.env.state));
  Serial.println();
}
