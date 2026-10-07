#include "pm_input.h"

#include <Arduino.h>

#include "config.h"
#include "pms7003.h"
#include "readiness.h"

namespace {
Pms7003Parser g_parser;
PmsFrame g_frame;
Readiness g_readiness;
uint32_t g_bootMs = 0;
}  // namespace

void pm_input::begin() {
  g_bootMs = millis();
  Serial1.begin(cfg::PMS_BAUD);
}

void pm_input::update(uint32_t nowMs, PmReading& out) {
  // Bound the work per call; the ring buffer holds 256 bytes anyway.
  for (int i = 0; i < 256 && Serial1.available() > 0; ++i) {
    if (g_parser.feed((uint8_t)Serial1.read(), g_frame)) {
      out.pm1 = g_frame.pm1Atm;
      out.pm25 = g_frame.pm25Atm;
      out.pm10 = g_frame.pm10Atm;
      readinessMarkValid(g_readiness, nowMs);
    }
  }

  out.state = readinessState(g_readiness, nowMs, g_bootMs, cfg::PMS_WARMUP_MS, cfg::PMS_STALE_MS);
}

uint32_t pm_input::framesOk() { return g_parser.framesOk(); }
uint32_t pm_input::checksumErrors() { return g_parser.checksumErrors(); }
