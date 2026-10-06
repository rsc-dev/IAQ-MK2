#include "env_sensors.h"

#include <Adafruit_BME680.h>
#include <Arduino.h>
#include <DFRobot_ENS160.h>
#include <Wire.h>

#include "config.h"

namespace {

Adafruit_BME680 g_bme(&Wire);
DFRobot_ENS160_I2C g_ens(&Wire, cfg::ENS160_ADDRESS);

bool g_bmePresent = false;
bool g_ensPresent = false;

uint32_t g_nextTickMs = 0;
bool g_readingInProgress = false;
uint32_t g_readingEndMs = 0;
uint32_t g_lastEnvOkMs = 0;
uint8_t g_envFailures = 0;
constexpr uint8_t kMaxEnvFailures = 3;

bool initBme() {
  for (const uint8_t addr : cfg::BME680_ADDRESSES) {
    if (!g_bme.begin(addr)) continue;
    g_bme.setTemperatureOversampling(BME680_OS_8X);
    g_bme.setHumidityOversampling(BME680_OS_2X);
    g_bme.setPressureOversampling(BME680_OS_4X);
    g_bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    g_bme.setGasHeater(320, 150);  // 320 C for 150 ms
    return true;
  }
  return false;
}

bool initEns() {
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (g_ens.begin() == NO_ERR) return true;
    delay(100);
  }
  return false;
}

void readEns(const EnvReading& env, EnsReading& out) {
  if (!g_ensPresent) return;
  if (env.state == SensorState::Ok) g_ens.setTempAndHum(env.temperatureC, env.humidityPct);

  const uint8_t validity = g_ens.getENS160Status();  // 0 normal, 1 warm-up, 2 initial start-up, 3 invalid
  out.validityFlag = validity;
  out.aqi = g_ens.getAQI();
  out.tvocPpb = g_ens.getTVOC();
  out.eco2Ppm = g_ens.getECO2();

  if (out.aqi < 1 || out.aqi > 5) {
    out.state = SensorState::Stale;  // nothing sensible on the bus
  } else if (validity == 0) {
    out.state = SensorState::Ok;
  } else if (validity == 1 || validity == 2) {
    out.state = SensorState::WarmingUp;
  } else {
    out.state = SensorState::Stale;
  }
}

void envFailed(EnvReading& env) {
  if (g_envFailures < 255) ++g_envFailures;
  if (g_envFailures >= kMaxEnvFailures && env.state == SensorState::Ok) env.state = SensorState::Stale;
}

}  // namespace

void env_sensors::begin(EnvReading& env, EnsReading& ens) {
  Wire.begin();
  g_bmePresent = initBme();
  g_ensPresent = initEns();
  env.state = g_bmePresent ? SensorState::NotReady : SensorState::Unavailable;
  ens.state = g_ensPresent ? SensorState::NotReady : SensorState::Unavailable;
  g_nextTickMs = millis();
}

void env_sensors::update(uint32_t nowMs, EnvReading& env, EnsReading& ens) {
  // Start a sampling tick.
  if (!g_readingInProgress && (int32_t)(nowMs - g_nextTickMs) >= 0) {
    g_nextTickMs = nowMs + cfg::ENV_INTERVAL_MS;
    if (g_bmePresent) {
      const uint32_t endMs = g_bme.beginReading();
      if (endMs == 0) {
        envFailed(env);
        readEns(env, ens);
      } else {
        g_readingInProgress = true;
        g_readingEndMs = endMs;
      }
    } else {
      readEns(env, ens);
    }
  }

  // Finish an in-flight BME680 reading.
  if (g_readingInProgress && (int32_t)(nowMs - g_readingEndMs) >= 0) {
    g_readingInProgress = false;
    if (g_bme.endReading()) {
      env.temperatureC = g_bme.temperature;
      env.humidityPct = g_bme.humidity;
      env.pressureHpa = (float)g_bme.pressure / 100.0f;
      env.gasKOhm = (float)g_bme.gas_resistance / 1000.0f;
      env.state = SensorState::Ok;
      g_lastEnvOkMs = nowMs;
      g_envFailures = 0;
    } else {
      envFailed(env);
    }
    readEns(env, ens);
  }

  if (env.state == SensorState::Ok && nowMs - g_lastEnvOkMs > cfg::ENV_STALE_MS) env.state = SensorState::Stale;
}

bool env_sensors::bmePresent() { return g_bmePresent; }
bool env_sensors::ensPresent() { return g_ensPresent; }
