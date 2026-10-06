#pragma once
#include <stdint.h>

// Non-blocking refresh of the Waveshare 2.9" (G) panel. requestRefresh() runs
// the (bounded) init sequence, sends the frame and triggers the refresh; update()
// polls BUSY from loop() and puts the panel to deep sleep when it finishes.
class EPaper {
 public:
  void begin();
  // Returns false if a refresh is still in progress.
  bool requestRefresh(const uint8_t* frameBuffer, bool fullRefresh);
  // Returns true exactly once when a refresh has completed (or timed out).
  bool update(uint32_t nowMs);

  bool busy() const { return state_ == State::Refreshing; }
  void trace();  // diagnostics: log BUSY pin transitions
  uint32_t refreshCount() const { return refreshCount_; }
  uint32_t lastDurationMs() const { return lastDurationMs_; }
  bool lastTimedOut() const { return lastTimedOut_; }
  bool lastWasFull() const { return lastFull_; }

  // Diagnostics for bring-up.
  uint8_t busyLevelAtRest() const { return busyAtRest_; }    // raw BUSY after begin(): 1 = HIGH (idle)
  uint32_t lastInitMs() const { return lastInitMs_; }          // time spent in the vendor init sequence
  bool busyAssertedAfterStart() const { return busyAfterStart_ != 0; }  // panel reported busy right after the refresh command
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
  uint8_t busyAtRest_ = 0;
  uint32_t lastInitMs_ = 0;
  uint8_t busyAfterStart_ = 0;
};
