#include "weather.h"

#include <Arduino.h>
#include <WiFiNINA.h>
#include <mbed.h>
#include <string.h>

#include "byte_source.h"
#include "config.h"
#include "forecast.h"
#include "http_body.h"
#include "net.h"
#include "serial_log.h"
#include "watchdog.h"

namespace {

constexpr uint32_t kFlagFetch = 1;   // main -> worker: start a fetch
constexpr uint32_t kFlagDone = 2;    // worker -> main: result ready

// Bytes from the TLS client with a per-byte wait and an overall deadline.
class ClientSource : public ByteSource {
 public:
  ClientSource(WiFiSSLClient& client, uint32_t deadlineMs) : client_(client), deadlineMs_(deadlineMs) {}
  int read() override {
    const uint32_t start = millis();
    while (client_.available() <= 0) {
      if (!client_.connected()) return -1;
      if (millis() - start > cfg::WEATHER_READ_TIMEOUT_MS) return -1;
      if ((int32_t)(millis() - deadlineMs_) >= 0) return -1;
      delay(2);
    }
    return client_.read();
  }

 private:
  WiFiSSLClient& client_;
  uint32_t deadlineMs_;
};

rtos::Thread* g_worker = nullptr;
rtos::EventFlags g_flags;
rtos::Mutex g_resultMutex;

// Written by the worker, read by update() under g_resultMutex.
OutdoorReading g_result;
bool g_resultOk = false;
char g_resultError[24] = "";
uint32_t g_resultDurationMs = 0;

uint32_t g_fetchesOk = 0;
uint32_t g_fetchesFailed = 0;
char g_lastError[24] = "";
bool g_busy = false;
bool g_everAttempted = false;
uint32_t g_lastAttemptMs = 0;
uint32_t g_lastOkMs = 0;
bool g_lastFailed = false;
char g_entry[1536];

// Runs in the worker thread. Returns an empty string on success.
const char* fetchOnce(OutdoorReading& out) {
  WiFiSSLClient client;
  client.setConnectionTimeout(cfg::WEATHER_TLS_CONNECT_TIMEOUT_MS);
  const uint32_t deadline = millis() + cfg::WEATHER_FETCH_TIMEOUT_MS;
  if (!client.connect(cfg::WEATHER_HOST, 443)) return "tls connect";

  client.print("GET ");
  client.print(cfg::WEATHER_PATH);
  client.print(" HTTP/1.1\r\nHost: ");
  client.print(cfg::WEATHER_HOST);
  client.print("\r\nUser-Agent: ");
  client.print(cfg::WEATHER_USER_AGENT);
  client.print("\r\nAccept: application/json\r\nConnection: close\r\n\r\n");

  ClientSource src(client, deadline);
  HttpResponseInfo info;
  if (!httpReadHeaders(src, info)) { client.stop(); return "headers"; }
  if (info.status != 200) {
    client.stop();
    static char err[24];
    snprintf(err, sizeof err, "http %d", info.status);
    return err;
  }
  HttpBody body(src, info);
  if (!forecastExtractFirstEntry(body, g_entry, sizeof g_entry)) { client.stop(); return "extract"; }
  client.stop();
  if (!forecastParseEntry(g_entry, out)) return "parse";
  return "";
}

void workerMain() {
  for (;;) {
    g_flags.wait_any(kFlagFetch);
    OutdoorReading result;
    const uint32_t start = millis();
    net::lockNina();
    const char* err = fetchOnce(result);
    net::unlockNina();
    g_resultMutex.lock();
    g_resultOk = err[0] == '\0';
    strncpy(g_resultError, err, sizeof g_resultError - 1);
    g_resultError[sizeof g_resultError - 1] = '\0';
    g_result = result;
    g_resultDurationMs = millis() - start;
    g_resultMutex.unlock();
    g_flags.set(kFlagDone);
  }
}

}  // namespace

void weather::begin() {
  if (!net::moduleAvailable()) return;
  g_worker = new rtos::Thread(osPriorityBelowNormal, 6144, nullptr, "weather");
  g_worker->start(workerMain);
}

void weather::update(uint32_t nowMs, OutdoorReading& out) {
  if (!net::moduleAvailable()) {
    out.state = SensorState::Unavailable;
    return;
  }

  // A NINA that stops answering mid-transfer parks the worker forever (the SPI
  // driver has no timeout) while it holds the NINA lock, which would silently
  // freeze network and clock. The only recovery is a reboot, which also resets
  // the module.
  if (g_busy && nowMs - g_lastAttemptMs > cfg::WEATHER_WORKER_HANG_MS) {
    serial_log::event("wx", "worker hung, rebooting");
    delay(50);
    NVIC_SystemReset();
  }

  // Collect a finished fetch.
  if (g_busy && (g_flags.get() & kFlagDone)) {
    g_flags.clear(kFlagDone);
    g_busy = false;
    g_resultMutex.lock();
    const bool ok = g_resultOk;
    OutdoorReading result = g_result;
    strncpy(g_lastError, g_resultError, sizeof g_lastError);
    const uint32_t duration = g_resultDurationMs;
    g_resultMutex.unlock();
    if (ok) {
      ++g_fetchesOk;
      g_lastFailed = false;
      g_lastOkMs = nowMs;
      result.state = SensorState::Ok;
      out = result;
      serial_log::eventValue("wx", "forecast ok, ms=", duration);
    } else {
      ++g_fetchesFailed;
      g_lastFailed = true;
      char msg[48];
      snprintf(msg, sizeof msg, "fetch failed (%s), ms=", g_lastError);
      serial_log::eventValue("wx", msg, duration);
    }
  }

  // Staleness of what is shown.
  if (out.state == SensorState::Ok && nowMs - g_lastOkMs > cfg::WEATHER_STALE_MS) out.state = SensorState::Stale;

  // Schedule the next fetch.
  if (g_busy || !net::connected()) return;
  const uint32_t wait = g_lastFailed ? cfg::WEATHER_RETRY_MS : cfg::WEATHER_INTERVAL_MS;
  if (g_everAttempted && nowMs - g_lastAttemptMs < wait) return;
  g_everAttempted = true;
  g_lastAttemptMs = nowMs;
  g_busy = true;
  serial_log::event("wx", "fetching forecast");
  g_flags.set(kFlagFetch);
}

uint32_t weather::fetchesOk() { return g_fetchesOk; }
uint32_t weather::fetchesFailed() { return g_fetchesFailed; }
const char* weather::lastError() { return g_lastError; }
