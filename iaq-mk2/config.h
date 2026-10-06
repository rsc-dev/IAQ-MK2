#pragma once
#include <stdint.h>

// All tunables live here. Pin numbers use Arduino numbering for the Nano RP2040 Connect.
namespace cfg {

// ---- Pins ------------------------------------------------------------------
constexpr uint8_t PIN_CO2_PWM  = 2;   // SEN0219 PWM output (3.3 V level)
constexpr uint8_t PIN_EPD_PWR  = 6;   // e-paper PWR (panel power enable)
constexpr uint8_t PIN_EPD_BUSY = 7;
constexpr uint8_t PIN_EPD_RST  = 8;
constexpr uint8_t PIN_EPD_DC   = 9;
constexpr uint8_t PIN_EPD_CS   = 10;
// e-paper DIN -> D11 (MOSI), CLK -> D13 (SCK): hardware SPI, fixed.
// PMS7003 TX -> D0 (Serial1 RX), PMS7003 RX -> D1 (Serial1 TX): hardware UART, fixed.
// BME680 + ENS160 -> A4 (SDA), A5 (SCL): hardware I2C, fixed.

constexpr uint8_t BME680_ADDRESSES[] = {0x76, 0x77};
constexpr uint8_t ENS160_ADDRESS = 0x53;   // DFRobot breakout default; 0x52 if ADDR is pulled low

// ---- Serial ----------------------------------------------------------------
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t BOOT_SERIAL_DELAY_MS = 2000;   // time for a terminal to attach before the [boot] lines
constexpr uint32_t PMS_BAUD    = 9600;

// ---- Timing ----------------------------------------------------------------
constexpr uint32_t ENV_INTERVAL_MS       = 2000;    // BME680 + ENS160 sampling
constexpr uint32_t LOG_INTERVAL_MS       = 5000;    // serial summary line
constexpr uint32_t DISPLAY_INTERVAL_MS   = 180000;  // Waveshare minimum is 180 s
constexpr uint32_t BOOT_SETTLE_MS        = 3000;    // sensor time before the first screen
constexpr uint8_t  DISPLAY_FULL_REFRESH_EVERY = 1;  // 1 = always full refresh; N > 1 = fast refresh in between
constexpr uint16_t DISPLAY_ROTATION      = 90;      // 90 or 270, depending on how the panel is mounted
constexpr bool     EPD_BUSY_ACTIVE_HIGH   = false;  // Waveshare G panels: BUSY is LOW while busy (verified: LOW for ~300 ms after RST)
constexpr bool     EPD_SOFTWARE_SPI       = false;  // bit-bang DIN/CLK instead of hardware SPI (diagnostic only)
constexpr uint32_t EPD_BUSY_TIMEOUT_MS    = 10000;  // vendor init / power-off busy waits
constexpr uint32_t EPD_REFRESH_TIMEOUT_MS = 45000;  // a full refresh takes ~16 s
constexpr uint32_t WATCHDOG_TIMEOUT_MS   = 8000;    // RP2040 hardware maximum is ~8.3 s

// ---- Warm-up and staleness ------------------------------------------------
constexpr uint32_t CO2_WARMUP_MS = 180000;  // SEN0219: 1 min preheat nominal, 3 min for stable output
constexpr uint32_t CO2_STALE_MS  = 5000;    // PWM cycle is ~1 s
constexpr uint16_t CO2_MIN_PLAUSIBLE_PPM = 300;
constexpr uint32_t PMS_WARMUP_MS = 30000;   // Plantower: >= 30 s after power-on
constexpr uint32_t PMS_STALE_MS  = 10000;   // frames arrive every 1-2.3 s
constexpr uint32_t ENV_STALE_MS  = 15000;

// ---- Thresholds ------------------------------------------------------------
constexpr uint16_t CO2_WARN_PPM   = 800;
constexpr uint16_t CO2_BAD_PPM    = 1200;
constexpr uint16_t PM25_WARN_UGM3 = 16;   // "> 15"
constexpr uint16_t PM25_BAD_UGM3  = 36;   // "> 35"
constexpr uint8_t  AQI_WARN = 2;
constexpr uint8_t  AQI_BAD  = 3;

}  // namespace cfg
