#include "pm_input.h"

#include <Arduino.h>

#include "config.h"
#include "pms7003.h"

namespace {
Pms7003Parser g_parser;
PmsFrame g_frame;
uint32_t g_lastFrameMs = 0;
bool g_everValid = false;
}  // namespace

void pm_input::begin() { Serial1.begin(cfg::PMS_BAUD); }

void pm_input::update(uint32_t nowMs, PmReading& out) {
  // Bound the work per call; the ring buffer holds 256 bytes anyway.
  for (int i = 0; i < 256 && Serial1.available() > 0; ++i) {
    if (g_parser.feed((uint8_t)Serial1.read(), g_frame)) {
      out.pm1 = g_frame.pm1Atm;
      out.pm25 = g_frame.pm25Atm;
      out.pm10 = g_frame.pm10Atm;
      g_lastFrameMs = nowMs;
      g_everValid = true;
    }
  }

  if (!g_everValid) {
    out.state = SensorState::NotReady;
    return;
  }
  if (nowMs - g_lastFrameMs > cfg::PMS_STALE_MS) {
    out.state = SensorState::Stale;
  } else if (nowMs < cfg::PMS_WARMUP_MS) {
    out.state = SensorState::WarmingUp;
  } else {
    out.state = SensorState::Ok;
  }
}

uint32_t pm_input::framesOk() { return g_parser.framesOk(); }
uint32_t pm_input::checksumErrors() { return g_parser.checksumErrors(); }
