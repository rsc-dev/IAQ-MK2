#include "net.h"

#include <Arduino.h>
#include <WiFiNINA.h>
#include <mbed.h>

#include <string.h>

#include "config.h"
#include "secrets.h"
#include "serial_log.h"

namespace {

enum class State : uint8_t { NoModule, Idle, Connecting, Connected, Backoff };

State g_state = State::NoModule;
uint32_t g_attemptStartMs = 0;
uint32_t g_backoffMs = cfg::WIFI_RETRY_MIN_MS;
uint32_t g_backoffUntilMs = 0;
uint32_t g_lastPollMs = 0;
uint32_t g_lastSyncMs = 0;
uint32_t g_epoch = 0;
uint32_t g_epochAtMs = 0;
int32_t g_rssi = 0;
char g_firmware[16] = "?";
rtos::Mutex g_ninaMutex;

void startBackoff(uint32_t nowMs, const char* why) {
  serial_log::eventValue("net", why, g_backoffMs / 1000);
  g_backoffUntilMs = nowMs + g_backoffMs;
  g_backoffMs = g_backoffMs >= cfg::WIFI_RETRY_MAX_MS / 2 ? cfg::WIFI_RETRY_MAX_MS : g_backoffMs * 2;
  g_state = State::Backoff;
}

void logConnected() {
  IPAddress ip = WiFi.localIP();
  char msg[64];
  snprintf(msg, sizeof msg, "connected %u.%u.%u.%u rssi=%ld dBm", ip[0], ip[1], ip[2], ip[3], (long)WiFi.RSSI());
  serial_log::event("net", msg);
}

}  // namespace

void net::begin() {
  if (WiFi.status() == WL_NO_MODULE) {
    g_state = State::NoModule;
    serial_log::event("net", "WiFi module not responding");
    return;
  }
  WiFi.setTimeout(cfg::WIFI_CONNECT_TIMEOUT_MS);
  strncpy(g_firmware, WiFi.firmwareVersion(), sizeof g_firmware - 1);
  serial_log::event("net", g_firmware);
  g_state = State::Idle;
}

void net::update(uint32_t nowMs) {
  if (g_state == State::NoModule) return;
  if (nowMs - g_lastPollMs < cfg::WIFI_POLL_MS) return;
  if (!tryLockNina()) return;  // the weather worker is using the module; poll later
  g_lastPollMs = nowMs;

  switch (g_state) {
    case State::Idle:
      serial_log::event("net", "connecting");
      WiFi.begin(SECRET_SSID, SECRET_PASS);  // returns after WIFI_CONNECT_TIMEOUT_MS at most
      g_attemptStartMs = nowMs;
      g_state = State::Connecting;
      break;

    case State::Connecting: {
      const uint8_t status = WiFi.status();
      if (status == WL_CONNECTED) {
        g_state = State::Connected;
        g_backoffMs = cfg::WIFI_RETRY_MIN_MS;
        g_lastSyncMs = 0;
        g_lastSyncMs = 0;
        logConnected();
      } else if (nowMs - g_attemptStartMs > cfg::WIFI_CONNECT_WINDOW_MS) {
        // Status values such as WL_NO_SSID_AVAIL are transient while the module scans,
        // so only the time window decides; the final status goes into the log.
        WiFi.disconnect();
        startBackoff(nowMs, status == WL_NO_SSID_AVAIL ? "network not found, retry in s=" : "connect failed, retry in s=");
      }
      break;
    }

    case State::Connected:
      if (WiFi.status() != WL_CONNECTED) {
        g_rssi = 0;
        startBackoff(nowMs, "connection lost, retry in s=");
      } else if (g_epoch == 0 || nowMs - g_lastSyncMs >= cfg::TIME_SYNC_INTERVAL_MS) {
        // Until the NINA's SNTP has answered once, ask at every poll; afterwards once a minute.
        g_lastSyncMs = nowMs;
        g_rssi = WiFi.RSSI();
        const uint32_t t = WiFi.getTime();
        if (t != 0) {
          if (g_epoch == 0) serial_log::eventValue("net", "time synced, epoch=", t);
          g_epoch = t;
          g_epochAtMs = nowMs;
        }
      }
      break;

    case State::Backoff:
      if ((int32_t)(nowMs - g_backoffUntilMs) >= 0) g_state = State::Idle;
      break;

    case State::NoModule:
      break;
  }
  unlockNina();
}

bool net::moduleAvailable() { return g_state != State::NoModule; }
bool net::connected() { return g_state == State::Connected; }

uint32_t net::epoch(uint32_t nowMs) {
  if (g_epoch == 0) return 0;
  return g_epoch + (nowMs - g_epochAtMs) / 1000u;
}

int32_t net::rssi() { return g_rssi; }
const char* net::firmwareVersion() { return g_firmware; }

const char* net::stateName() {
  switch (g_state) {
    case State::NoModule: return "no-module";
    case State::Idle:
    case State::Connecting: return "connecting";
    case State::Connected: return "connected";
    case State::Backoff: return "retry";
  }
  return "?";
}

bool net::tryLockNina() { return g_ninaMutex.trylock(); }
void net::lockNina() { g_ninaMutex.lock(); }
void net::unlockNina() { g_ninaMutex.unlock(); }
