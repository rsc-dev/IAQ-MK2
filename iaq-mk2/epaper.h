#pragma once
#include <stdint.h>

#include "dashboard.h"

// Non-blocking refresh of the Waveshare 2.9" (B) V4 panel (SSD1680, black/white/red).
// requestRefresh() splits the 2-bpp frame into the panel's two planes, runs the
// (bounded) init sequence, sends the planes and triggers the update; update()
// polls BUSY from loop() and puts the panel to deep sleep when it finishes.
class EPaper {
 public:
  void begin();
  // Returns false if a refresh is still in progress.
  bool requestRefresh(const uint8_t* frameBuffer, bool fullRefresh);
  // Returns true exactly once when a refresh has completed (or timed out).
  bool update(uint32_t nowMs);
  void trace();  // diagnostics: log BUSY pin transitions

  bool busy() const { return state_ == State::Refreshing; }
  uint32_t refreshCount() const { return refreshCount_; }
  uint32_t lastDurationMs() const { return lastDurationMs_; }
  bool lastTimedOut() const { return lastTimedOut_; }
  bool lastWasFull() const { return lastFull_; }

  // Diagnostics for bring-up.
  bool busyAtRest() const { return busyAtRest_; }               // panel reported busy right after begin()
  uint32_t lastInitMs() const { return lastInitMs_; }           // time spent in the vendor init sequence
  bool busyAssertedAfterStart() const { return busyAfterStart_; }  // panel busy right after the refresh command
  uint32_t busyTimeouts() const;                                // vendor busy waits that timed out

 private:
  enum class State : uint8_t { Idle, Refreshing };
  static constexpr uint32_t kBusySettleMs = 200;  // BUSY asserts shortly after the refresh command

  State state_ = State::Idle;
  uint32_t startedMs_ = 0;
  uint32_t refreshCount_ = 0;
  uint32_t lastDurationMs_ = 0;
  bool lastTimedOut_ = false;
  bool lastFull_ = true;
  bool busyAtRest_ = false;
  uint32_t lastInitMs_ = 0;
  bool busyAfterStart_ = false;
  uint8_t blackPlane_[kPlaneSize];
  uint8_t redPlane_[kPlaneSize];
};
