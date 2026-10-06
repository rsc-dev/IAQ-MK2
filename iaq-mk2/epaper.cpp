#include "epaper.h"

#include <Arduino.h>

#include "config.h"
#include "src/waveshare/DEV_Config.h"
#include "src/waveshare/EPD_2in9g.h"

void EPaper::trace() { DEV_Busy_Trace(); }

void EPaper::begin() {
  DEV_Module_Init();
  delay(10);
  busyAtRest_ = EPD_2IN9G_BusyLevel();
}

uint32_t EPaper::busyTimeouts() const { return EPD_2IN9G_BusyTimeouts(); }

bool EPaper::requestRefresh(const uint8_t* frameBuffer, bool fullRefresh) {
  if (state_ != State::Idle) return false;
  Serial.print("[epd] init begin at ms=");
  Serial.println(millis());
  const uint32_t initStart = millis();
  if (fullRefresh) {
    EPD_2IN9G_Init();
  } else {
    EPD_2IN9G_Init_Fast();
  }
  lastInitMs_ = millis() - initStart;
  Serial.print("[epd] init done, sending frame at ms=");
  Serial.println(millis());
  EPD_2IN9G_Display_Start(frameBuffer);
  startedMs_ = millis();
  Serial.print("[epd] refresh command sent at ms=");
  Serial.println(startedMs_);
  delay(20);
  busyAfterStart_ = EPD_2IN9G_IsBusy();
  lastFull_ = fullRefresh;
  state_ = State::Refreshing;
  return true;
}

bool EPaper::update(uint32_t nowMs) {
  if (state_ != State::Refreshing) return false;
  const uint32_t elapsed = nowMs - startedMs_;
  if (elapsed < kBusySettleMs) return false;
  const bool stillBusy = EPD_2IN9G_IsBusy() != 0;
  if (stillBusy && elapsed < cfg::EPD_REFRESH_TIMEOUT_MS) return false;

  lastTimedOut_ = stillBusy;
  lastDurationMs_ = elapsed;
  ++refreshCount_;
  Serial.print("[epd] sleep at ms=");
  Serial.println(millis());
  EPD_2IN9G_Sleep();
  state_ = State::Idle;
  return true;
}
