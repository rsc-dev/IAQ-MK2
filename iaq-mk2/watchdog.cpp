#include "watchdog.h"

#include <Arduino.h>
#include <mbed.h>

namespace {
bool g_started = false;
bool g_suspended = false;
}

void watchdog::begin(uint32_t timeoutMs) {
  mbed::Watchdog& wd = mbed::Watchdog::get_instance();
  const uint32_t maxMs = wd.get_max_timeout();
  g_started = wd.start(timeoutMs > maxMs ? maxMs : timeoutMs);
}

void watchdog::kick() {
  if (g_started && !g_suspended) mbed::Watchdog::get_instance().kick();
}

void watchdog::suspend() { g_suspended = true; }

bool watchdog::active() { return g_started && !g_suspended; }
