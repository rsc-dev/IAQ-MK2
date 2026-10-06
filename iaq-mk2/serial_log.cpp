#include "serial_log.h"

#include <Arduino.h>
#include <USB/PluggableUSBSerial.h>  // _SerialUSB

#include "air_status.h"
#include "co2_input.h"
#include "config.h"
#include "pm_input.h"
#include "text_format.h"
#include "watchdog.h"

namespace {
// The IDE/arduino-cli request a reset into the bootloader by opening the port at
// 1200 baud. The core normally handles that in its own thread by calling the
// bootrom's reset_usb_boot(), which re-arms the hardware watchdog with a ~10 ms
// countdown; our regular watchdog kicks would cancel that forever. So the
// request is handled here: stop kicking, wait for the host to close the port
// (as the core does), then enter the bootloader.
volatile bool g_bootloaderRequested = false;

void onLineCoding(int baud, int /*bits*/, int /*parity*/, int /*stop*/) {
  if (baud == 1200) g_bootloaderRequested = true;
}

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

void serial_log::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  _SerialUSB.attach(onLineCoding);  // the USB CDC object behind Serial; replaces the core's handler
}

void serial_log::update() {
  if (!g_bootloaderRequested) return;
  watchdog::suspend();
  Serial.println("[boot] bootloader requested, resetting");
  const uint32_t start = millis();
  while (_SerialUSB.connected() || millis() - start < 200) delay(1);
  _ontouch1200bps_();  // core variant: reset_usb_boot()
}

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
