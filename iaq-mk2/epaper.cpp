#include "epaper.h"

#include <Arduino.h>

#include "config.h"
#include "src/waveshare/DEV_Config.h"
#include "src/waveshare/EPD_2in9b_V4.h"

void EPaper::trace() { DEV_Busy_Trace(); }

void EPaper::begin() {
  DEV_Module_Init();
  delay(10);
  busyAtRest_ = EPD_2IN9B_V4_IsBusy() != 0;
}

uint32_t EPaper::busyTimeouts() const { return EPD_2IN9B_V4_BusyTimeouts(); }

bool EPaper::requestRefresh(const uint8_t* frameBuffer, bool fullRefresh) {
  if (state_ != State::Idle) return false;
  dashboardSplitPlanes(frameBuffer, blackPlane_, redPlane_);

  Serial.print("[epd] init begin at ms=");
  Serial.println(millis());
  const uint32_t initStart = millis();
  if (fullRefresh) {
    EPD_2IN9B_V4_Init();
  } else {
    EPD_2IN9B_V4_Init_Fast();
  }
  lastInitMs_ = millis() - initStart;
  EPD_2IN9B_V4_Display_Start(blackPlane_, redPlane_, fullRefresh ? 0 : 1);
  startedMs_ = millis();
  delay(20);
  busyAfterStart_ = EPD_2IN9B_V4_IsBusy() != 0;
  lastFull_ = fullRefresh;
  state_ = State::Refreshing;
  return true;
}

bool EPaper::update(uint32_t nowMs) {
  if (state_ != State::Refreshing) return false;
  const uint32_t elapsed = nowMs - startedMs_;
  if (elapsed < kBusySettleMs) return false;
  const bool stillBusy = EPD_2IN9B_V4_IsBusy() != 0;
  if (stillBusy && elapsed < cfg::EPD_REFRESH_TIMEOUT_MS) return false;

  lastTimedOut_ = stillBusy;
  lastDurationMs_ = elapsed;
  ++refreshCount_;
  EPD_2IN9B_V4_Sleep();
  state_ = State::Idle;
  return true;
}
