#include "epaper.h"

#include <Arduino.h>

#include "config.h"
#include "src/waveshare/DEV_Config.h"
#include "src/waveshare/EPD_2in9g.h"

void EPaper::begin() { DEV_Module_Init(); }

bool EPaper::requestRefresh(const uint8_t* frameBuffer, bool fullRefresh) {
  if (state_ != State::Idle) return false;
  if (fullRefresh) {
    EPD_2IN9G_Init();
  } else {
    EPD_2IN9G_Init_Fast();
  }
  EPD_2IN9G_Display_Start(frameBuffer);
  startedMs_ = millis();
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
  EPD_2IN9G_Sleep();
  state_ = State::Idle;
  return true;
}
