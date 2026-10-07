#pragma once
#include <stdint.h>

// WiFi on the NINA-W102 (WiFiNINA). Connects with a short per-call timeout
// and polls the status from loop(), retries with exponential backoff, and
// keeps the NTP epoch the module provides. All NINA traffic from other
// threads must hold the NINA lock (see lockNina / unlockNina).
namespace net {
void begin();                       // credentials from secrets.h
void update(uint32_t nowMs);
bool moduleAvailable();
bool connected();
uint32_t epoch(uint32_t nowMs);     // Unix time now, 0 if unknown
int32_t rssi();                     // last polled RSSI in dBm (0 when not connected)
const char* firmwareVersion();
const char* stateName();            // "no-module", "connecting", "connected", "retry"

// Serializes NINA access between loop() and the weather worker thread.
bool tryLockNina();
void lockNina();
void unlockNina();
}  // namespace net
