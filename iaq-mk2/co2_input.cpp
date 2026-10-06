#include "co2_input.h"

#include <Arduino.h>

#include "config.h"
#include "sen0219.h"

namespace {

// Written by the ISR, read by update() with interrupts disabled.
volatile uint32_t g_riseUs = 0;
volatile uint32_t g_fallUs = 0;
volatile uint32_t g_highUs = 0;
volatile uint32_t g_lowUs = 0;
volatile bool g_haveRise = false;
volatile bool g_haveFall = false;
volatile bool g_haveHigh = false;
volatile uint32_t g_cycleSeq = 0;  // increments when (g_highUs, g_lowUs) form a complete cycle

uint32_t g_lastSeq = 0;
uint32_t g_lastValidMs = 0;
bool g_everValid = false;
uint16_t g_lastPpm = 0;
uint32_t g_cyclesSeen = 0;
uint32_t g_cyclesRejected = 0;

void onEdge() {
  const uint32_t now = micros();
  if (digitalRead(cfg::PIN_CO2_PWM) == HIGH) {
    // Rising edge closes a low phase; together with the preceding high phase it is one cycle.
    if (g_haveFall && g_haveHigh) {
      g_lowUs = now - g_fallUs;
      g_cycleSeq = g_cycleSeq + 1;
    }
    g_riseUs = now;
    g_haveRise = true;
  } else {
    if (g_haveRise) {
      g_highUs = now - g_riseUs;
      g_haveHigh = true;
    }
    g_fallUs = now;
    g_haveFall = true;
  }
}

}  // namespace

void co2_input::begin() {
  pinMode(cfg::PIN_CO2_PWM, INPUT);
  attachInterrupt(digitalPinToInterrupt(cfg::PIN_CO2_PWM), onEdge, CHANGE);
}

void co2_input::update(uint32_t nowMs, Co2Reading& out) {
  noInterrupts();
  const uint32_t seq = g_cycleSeq;
  const uint32_t highUs = g_highUs;
  const uint32_t lowUs = g_lowUs;
  interrupts();

  if (seq != g_lastSeq) {
    g_lastSeq = seq;
    ++g_cyclesSeen;
    const Co2Result r = co2FromPulse(highUs, lowUs);
    if (r.valid) {
      g_lastPpm = r.ppm;
      g_lastValidMs = nowMs;
      g_everValid = true;
    } else {
      ++g_cyclesRejected;
    }
  }

  if (!g_everValid) {
    out.ppm = 0;
    out.state = SensorState::NotReady;
    return;
  }
  out.ppm = g_lastPpm;
  if (nowMs - g_lastValidMs > cfg::CO2_STALE_MS) {
    out.state = SensorState::Stale;
  } else if (nowMs < cfg::CO2_WARMUP_MS || g_lastPpm < cfg::CO2_MIN_PLAUSIBLE_PPM) {
    out.state = SensorState::WarmingUp;
  } else {
    out.state = SensorState::Ok;
  }
}

uint32_t co2_input::cyclesSeen() { return g_cyclesSeen; }
uint32_t co2_input::cyclesRejected() { return g_cyclesRejected; }
