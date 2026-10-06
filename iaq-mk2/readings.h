#pragma once
#include <stdint.h>

enum class SensorState : uint8_t {
  NotReady = 0,  // no measurement received yet
  WarmingUp,     // measuring, but not trustworthy yet
  Ok,
  Stale,         // was OK, no data within the timeout
  Unavailable,   // not detected at boot
};

inline const char* sensorStateName(SensorState s) {
  switch (s) {
    case SensorState::NotReady:    return "init";
    case SensorState::WarmingUp:   return "warmup";
    case SensorState::Ok:          return "ok";
    case SensorState::Stale:       return "stale";
    case SensorState::Unavailable: return "n/a";
  }
  return "?";
}

struct Co2Reading {
  uint16_t ppm = 0;
  SensorState state = SensorState::NotReady;
};

struct PmReading {
  uint16_t pm1 = 0;
  uint16_t pm25 = 0;
  uint16_t pm10 = 0;
  SensorState state = SensorState::NotReady;
};

struct EnsReading {
  uint8_t aqi = 0;          // 1..5 (UBA index)
  uint16_t tvocPpb = 0;
  uint16_t eco2Ppm = 0;     // logged only, never displayed as CO2
  uint8_t validityFlag = 0; // 0 normal, 1 warm-up, 2 initial start-up, 3 invalid
  SensorState state = SensorState::NotReady;
};

struct EnvReading {
  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHpa = 0.0f;
  float gasKOhm = 0.0f;
  SensorState state = SensorState::NotReady;
};

struct Readings {
  Co2Reading co2;
  PmReading pm;
  EnsReading ens;
  EnvReading env;
};
