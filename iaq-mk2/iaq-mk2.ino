/*
 * IAQ-MK2: indoor air-quality station.
 * Arduino Nano RP2040 Connect + BME680 + ENS160 + PMS7003 + SEN0219 +
 * Waveshare 2.9" e-Paper (G). See README.md for wiring and build steps.
 *
 * loop() is cooperative: inputs update `readings`, the dashboard and the
 * serial log are pure functions of it, and the e-paper refresh is polled.
 */
#include <Arduino.h>

#include "co2_input.h"
#include "config.h"
#include "dashboard.h"
#include "env_sensors.h"
#include "epaper.h"
#include "pm_input.h"
#include "readings.h"
#include "serial_log.h"
#include "watchdog.h"

namespace {
Readings readings;
EPaper epaper;
uint8_t frameBuffer[kFrameBufferSize];

uint32_t lastLogMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t displayRequests = 0;

uint32_t uptimeSeconds(uint32_t nowMs) { return nowMs / 1000; }

void pollInputs(uint32_t nowMs) {
  pm_input::update(nowMs, readings.pm);
  co2_input::update(nowMs, readings.co2);
  env_sensors::update(nowMs, readings.env, readings.ens);
}

void refreshDisplay(uint32_t nowMs) {
  const bool full = cfg::DISPLAY_FULL_REFRESH_EVERY <= 1 ||
                    (displayRequests % cfg::DISPLAY_FULL_REFRESH_EVERY) == 0;
  dashboardRender(readings, uptimeSeconds(nowMs), frameBuffer);
  if (epaper.requestRefresh(frameBuffer, full)) {
    ++displayRequests;
    serial_log::event("epd", full ? "refresh started (full)" : "refresh started (fast)");
    serial_log::eventValue("epd", "init ms=", epaper.lastInitMs());
    serial_log::eventValue("epd", "busy-wait timeouts so far=", epaper.busyTimeouts());
    serial_log::event("epd", epaper.busyAssertedAfterStart() ? "panel busy after refresh cmd (good)"
                                                              : "panel NOT busy after refresh cmd (check BUSY wiring/polarity)");
  } else {
    serial_log::event("warn", "display busy, refresh skipped");
  }
}
}  // namespace

void setup() {
  serial_log::begin();
  watchdog::begin(cfg::WATCHDOG_TIMEOUT_MS);
  delay(cfg::BOOT_SERIAL_DELAY_MS);  // give a connected terminal a moment; never wait for one

  serial_log::event("boot", "IAQ-MK2 air quality station");
  serial_log::event("boot", watchdog::active() ? "watchdog armed" : "watchdog unavailable");

  env_sensors::begin(readings.env, readings.ens);
  serial_log::event("boot", env_sensors::bmePresent() ? "BME680 found" : "BME680 NOT found");
  serial_log::event("boot", env_sensors::ensPresent() ? "ENS160 found" : "ENS160 NOT found");

  pm_input::begin();
  co2_input::begin();
  epaper.begin();
  serial_log::event("boot", "PMS7003 UART, SEN0219 PWM capture and e-paper initialised");
  serial_log::event("boot", epaper.busyAtRest() ? "e-paper reports busy at rest (unexpected)" : "e-paper idle at rest");

  // Let the fast sensors deliver first values before the boot screen.
  const uint32_t settleStart = millis();
  while (millis() - settleStart < cfg::BOOT_SETTLE_MS) {
    watchdog::kick();
    pollInputs(millis());
    delay(10);
  }

  const uint32_t now = millis();
  serial_log::summary(readings, uptimeSeconds(now));
  refreshDisplay(now);
  lastDisplayMs = now;
  lastLogMs = now;
}

void loop() {
  const uint32_t now = millis();
  watchdog::kick();
  serial_log::update();

  pollInputs(now);

  epaper.trace();
  if (epaper.update(now)) {
    serial_log::eventValue("epd", epaper.lastTimedOut() ? "refresh TIMED OUT after ms=" : "refresh done, ms=",
                           epaper.lastDurationMs());
  }

  if (now - lastLogMs >= cfg::LOG_INTERVAL_MS) {
    lastLogMs = now;
    serial_log::summary(readings, uptimeSeconds(now));
  }

  if (now - lastDisplayMs >= cfg::DISPLAY_INTERVAL_MS && !epaper.busy()) {
    lastDisplayMs = now;
    refreshDisplay(now);
  }

  // Yield to the mbed RTOS once per iteration (USB, timers, idle thread).
  delay(1);
}
