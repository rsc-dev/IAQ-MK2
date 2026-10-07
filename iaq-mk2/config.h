#pragma once
#include <stdint.h>

// All tunables live here. Pin numbers use Arduino numbering for the Nano RP2040 Connect.

#define IAQ_MK2_VERSION "1.0.0"   // bump with each tagged release

namespace cfg {

constexpr const char* FIRMWARE_VERSION = IAQ_MK2_VERSION;

// ---- Pins ------------------------------------------------------------------
constexpr uint8_t PIN_CO2_PWM  = 2;   // SEN0219 PWM output (3.3 V level)
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
constexpr uint32_t DISPLAY_INTERVAL_MS   = 180000;  // Waveshare minimum is 180 s (2.9" (B) V4, full refresh ~14-16 s)
constexpr uint32_t BOOT_SETTLE_MS        = 3000;    // sensor time before the first screen
constexpr uint8_t  DISPLAY_FULL_REFRESH_EVERY = 1;  // 1 = always full refresh; N > 1 = vendor fast mode in between (not verified)
constexpr uint16_t DISPLAY_ROTATION      = 90;      // 90 or 270, depending on how the panel is mounted
constexpr uint32_t EPD_BUSY_TIMEOUT_MS    = 3000;   // vendor init busy waits (SWRESET etc. take ~10 ms)
constexpr uint32_t EPD_REFRESH_TIMEOUT_MS = 45000;  // a full refresh takes ~14-16 s
constexpr uint32_t WATCHDOG_TIMEOUT_MS   = 8000;    // RP2040 hardware maximum is ~8.3 s

// ---- Network and forecast --------------------------------------------------
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 1000;      // per WiFi.begin() call; status is polled afterwards
constexpr uint32_t WIFI_CONNECT_WINDOW_MS  = 30000;     // give up on an attempt after this
constexpr uint32_t WIFI_RETRY_MIN_MS       = 10000;     // exponential backoff between attempts
constexpr uint32_t WIFI_RETRY_MAX_MS       = 600000;
constexpr uint32_t WIFI_POLL_MS            = 500;       // status / NTP polling cadence
constexpr uint32_t TIME_SYNC_INTERVAL_MS   = 60000;     // re-read the NTP epoch from the NINA
constexpr const char* WEATHER_HOST = "api.met.no";
constexpr const char* WEATHER_PATH = "/weatherapi/locationforecast/2.0/compact?lat=54.468&lon=18.495";  // Gdynia Wielki Kack
constexpr const char* WEATHER_USER_AGENT = "IAQ-MK2/" IAQ_MK2_VERSION " github.com/rsc-dev/IAQ-MK2";
constexpr uint32_t WEATHER_INTERVAL_MS     = 3600000;   // MET Norway asks not to poll faster than Expires (~30 min)
constexpr uint32_t WEATHER_RETRY_MS        = 600000;    // after a failed fetch
constexpr uint32_t WEATHER_STALE_MS        = 10800000;  // 3 h without a good reply -> "no forecast"
constexpr uint32_t WEATHER_READ_TIMEOUT_MS = 5000;      // no byte for this long aborts a fetch
constexpr uint32_t WEATHER_FETCH_TIMEOUT_MS = 30000;    // whole request
constexpr uint32_t WEATHER_TLS_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WEATHER_WORKER_HANG_MS = 90000;       // worker still busy after this: the NINA stopped answering, reboot

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
