# IAQ-MK2 Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware for an Arduino Nano RP2040 Connect air-quality station (BME680, ENS160, PMS7003, SEN0219) with a Waveshare 2.9" four-colour e-paper status panel.

**Architecture:** A cooperative `loop()` updates one `Readings` struct from four input modules; the dashboard renderer and serial logger are pure functions of that struct. Pure logic (parsers, conversions, thresholds, formatting, layout) has no Arduino dependency and is unit-tested on the host with g++; Arduino glue is kept thin and compile-checked with arduino-cli. The e-paper refresh is a non-blocking state machine over the vendored Waveshare driver.

**Tech Stack:** Arduino `arduino:mbed_nano` 4.5.0 core, Adafruit BME680 2.0.6 (+ Unified Sensor 1.1.15, BusIO 1.17.4), DFRobot_ENS160 1.0.1, vendored Waveshare EPD_2in9g/GUI_Paint, g++ host tests, arduino-cli, uv + Pillow for preview PNGs.

**Spec:** `docs/superpowers/specs/2026-10-06-iaq-mk2-firmware-design.md`

## Global Constraints

- Board: `arduino:mbed_nano:nanorp2040connect`, core 4.5.0. Library versions pinned as above in `iaq-mk2/sketch.yaml`.
- Sketch folder `iaq-mk2/` (Arduino requires the folder name to equal the `.ino` name). Vendor code in `iaq-mk2/src/waveshare/`.
- E-paper: refresh interval 180 s minimum, deep sleep after each refresh, re-init before each refresh, PWR pin driven high.
- Framebuffer is 9472 bytes; all drawing coordinates must satisfy 0 <= x <= 295 and 0 <= y <= 127 (vendor bounds check is off by one).
- Text through `Paint_DrawChar` only (correct fg/bg order); never `Paint_DrawString_EN`. Fonts cover ASCII 0x20-0x7E only.
- No `delay()` in `loop()`; only vendor init/sleep sequences may block, and they kick the watchdog through `DEV_Delay_ms`.
- No `%f`, `%*d` or `%0Nd` in `snprintf`; floats go through `formatFixed`.
- Every reading carries a `SensorState`; nothing is shown as valid after its source stops.
- Pure modules (`text_format`, `air_status`, `pms7003`, `sen0219`, `dashboard`, `readings.h`, `config.h`) must compile on the host without Arduino headers.
- Commits: one per task, message prefix `feat:`/`test:`/`docs:`/`vendor:`, trailer `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.

## Review Focus

1. PMS7003 byte stream with noise, truncated frames, a `0x42 0x42 0x4D` start and a wrong length field must resynchronise without dropping the next good frame (test in Task 4).
2. SEN0219 pulse timing around `micros()` wrap-around, periods outside 1004 ms +-10 %, and a high pulse shorter than 2 ms must yield "invalid", never a negative or absurd ppm (test in Task 5).
3. A sensor that worked and then stops must become `Stale` within its timeout and render as `--` (tests in Task 3 for levels; glue logic in Tasks 8-9 is reviewed by reading).
4. Extreme values (CO2 5000, PM2.5 999, TVOC 65000, gas 9999 kOhm, pressure 1099.9, temperature -10.5) must not write outside the framebuffer or wrap onto other rows (canary test in Task 7).
5. NaN or negative floats in the log and on the panel must format as `--` or `-3.5`, never garbage (test in Task 2).

---

### Task 1: Scaffold, config, readings, host test harness

**Files:**
- Create: `iaq-mk2/config.h`, `iaq-mk2/readings.h`, `iaq-mk2/sketch.yaml`, `tests/Makefile`, `tests/test.h`, `tests/test_main.cpp`, `.gitignore`

**Interfaces:**
- Produces: `namespace cfg` constants; `enum class SensorState`; structs `Co2Reading`, `PmReading`, `EnsReading`, `EnvReading`, `Readings`; `sensorStateName(SensorState) -> const char*`; test macros `TEST(name)`, `CHECK(cond)`, `CHECK_EQ(a,b)`, `CHECK_STR_EQ(a,b)`, `CHECK_NEAR(a,b,eps)`.

- [ ] **Step 1: Create the files**

```bash
mkdir -p iaq-mk2/src/waveshare tests
cat > .gitignore <<'EOF'
tests/build/
tests/out/
build/
*.uf2
*.elf
*.bin
EOF
cat > iaq-mk2/config.h <<'EOF'
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
constexpr uint32_t PMS_BAUD    = 9600;

// ---- Timing ----------------------------------------------------------------
constexpr uint32_t ENV_INTERVAL_MS       = 2000;    // BME680 + ENS160 sampling
constexpr uint32_t LOG_INTERVAL_MS       = 5000;    // serial summary line
constexpr uint32_t DISPLAY_INTERVAL_MS   = 180000;  // Waveshare minimum is 180 s
constexpr uint32_t BOOT_SETTLE_MS        = 3000;    // sensor time before the first screen
constexpr uint8_t  DISPLAY_FULL_REFRESH_EVERY = 1;  // 1 = always full refresh; N > 1 = fast refresh in between
constexpr uint16_t DISPLAY_ROTATION      = 90;      // 90 or 270, depending on how the panel is mounted
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
EOF
cat > iaq-mk2/readings.h <<'EOF'
#pragma once
#include <stdint.h>

enum class SensorState : uint8_t {
  NotReady = 0,  // no measurement received yet
  WarmingUp,     // measuring, but not trustworthy yet
  Ok,
  Stale,         // was OK, no data within the timeout
  Unavailable,   // not detected at boot
};

inline const char* sensorStateName(SensorState s) {
  switch (s) {
    case SensorState::NotReady:    return "init";
    case SensorState::WarmingUp:   return "warmup";
    case SensorState::Ok:          return "ok";
    case SensorState::Stale:       return "stale";
    case SensorState::Unavailable: return "n/a";
  }
  return "?";
}

struct Co2Reading {
  uint16_t ppm = 0;
  SensorState state = SensorState::NotReady;
};

struct PmReading {
  uint16_t pm1 = 0;
  uint16_t pm25 = 0;
  uint16_t pm10 = 0;
  SensorState state = SensorState::NotReady;
};

struct EnsReading {
  uint8_t aqi = 0;          // 1..5 (UBA index)
  uint16_t tvocPpb = 0;
  uint16_t eco2Ppm = 0;     // logged only, never displayed as CO2
  uint8_t validityFlag = 0; // 0 normal, 1 warm-up, 2 initial start-up, 3 invalid
  SensorState state = SensorState::NotReady;
};

struct EnvReading {
  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHpa = 0.0f;
  float gasKOhm = 0.0f;
  SensorState state = SensorState::NotReady;
};

struct Readings {
  Co2Reading co2;
  PmReading pm;
  EnsReading ens;
  EnvReading env;
};
EOF
cat > iaq-mk2/sketch.yaml <<'EOF'
# arduino-cli build profile. Usage from the repo root:
#   arduino-cli compile --profile nanorp2040 iaq-mk2
#   arduino-cli upload  --profile nanorp2040 -p /dev/ttyACM0 iaq-mk2
profiles:
  nanorp2040:
    fqbn: arduino:mbed_nano:nanorp2040connect
    platforms:
      - platform: arduino:mbed_nano (4.5.0)
    libraries:
      - Adafruit BME680 Library (2.0.6)
      - Adafruit Unified Sensor (1.1.15)
      - Adafruit BusIO (1.17.4)
      - DFRobot_ENS160 (1.0.1)
EOF
cat > tests/test.h <<'EOF'
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>

struct TestCase {
  const char* name;
  void (*fn)();
  TestCase* next;
};

void registerTest(TestCase* tc);
int runAllTests();
extern int g_failures;

#define TEST(name)                                                        \
  static void name();                                                     \
  static TestCase name##_case{#name, name, nullptr};                      \
  static struct name##_registrar {                                        \
    name##_registrar() { registerTest(&name##_case); }                    \
  } name##_registrar_instance;                                            \
  static void name()

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                    \
  do {                                                                    \
    const long long _a = (long long)(a);                                  \
    const long long _b = (long long)(b);                                  \
    if (_a != _b) {                                                       \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__,   \
                  __LINE__, #a, #b, _a, _b);                              \
    }                                                                     \
  } while (0)

#define CHECK_STR_EQ(a, b)                                                \
  do {                                                                    \
    const char* _a = (a);                                                 \
    const char* _b = (b);                                                 \
    if (std::strcmp(_a, _b) != 0) {                                       \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n",          \
                  __FILE__, __LINE__, #a, #b, _a, _b);                    \
    }                                                                     \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                             \
  do {                                                                    \
    const double _a = (a);                                                \
    const double _b = (b);                                                \
    if (std::fabs(_a - _b) > (eps)) {                                     \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s ~= %s (%g vs %g)\n", __FILE__,        \
                  __LINE__, #a, #b, _a, _b);                              \
    }                                                                     \
  } while (0)
EOF
cat > tests/test_main.cpp <<'EOF'
#include "test.h"

int g_failures = 0;
static TestCase* g_tests = nullptr;

void registerTest(TestCase* tc) {
  tc->next = g_tests;
  g_tests = tc;
}

int runAllTests() {
  int count = 0;
  for (TestCase* tc = g_tests; tc; tc = tc->next) {
    const int before = g_failures;
    tc->fn();
    ++count;
    std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", tc->name);
  }
  std::printf("%d tests, %d failures\n", count, g_failures);
  return g_failures == 0 ? 0 : 1;
}

int main() { return runAllTests(); }
EOF
cat > tests/Makefile <<'EOF'
# Host-side unit tests and dashboard previews for IAQ-MK2.
#   make        build + run tests
#   make preview render dashboard PNGs into out/
CXX      ?= g++
SKETCH   := ../iaq-mk2
VENDOR   := $(SKETCH)/src/waveshare
CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -Werror -I$(SKETCH) -I$(VENDOR) -DIAQ_HOST_BUILD
VENDOR_FLAGS := -std=c++17 -O1 -g -w -I$(SKETCH) -I$(VENDOR) -DIAQ_HOST_BUILD

PURE_SRC   := $(wildcard $(SKETCH)/text_format.cpp $(SKETCH)/air_status.cpp $(SKETCH)/pms7003.cpp $(SKETCH)/sen0219.cpp $(SKETCH)/dashboard.cpp)
VENDOR_SRC := $(wildcard $(VENDOR)/GUI_Paint.cpp $(VENDOR)/font8.cpp $(VENDOR)/font12.cpp $(VENDOR)/font16.cpp $(VENDOR)/font20.cpp $(VENDOR)/font24.cpp)
TEST_SRC   := $(wildcard test_*.cpp)

PURE_OBJ   := $(patsubst $(SKETCH)/%.cpp,build/pure_%.o,$(PURE_SRC))
VENDOR_OBJ := $(patsubst $(VENDOR)/%.cpp,build/vendor_%.o,$(VENDOR_SRC))
TEST_OBJ   := $(patsubst %.cpp,build/%.o,$(TEST_SRC))

.PHONY: all test preview clean
all: test

test: build/test_runner
	./build/test_runner

preview: build/render_preview
	mkdir -p out
	./build/render_preview out
	uv run --quiet ppm_to_png.py out

build:
	mkdir -p build

build/pure_%.o: $(SKETCH)/%.cpp | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/vendor_%.o: $(VENDOR)/%.cpp | build
	$(CXX) $(VENDOR_FLAGS) -c -o $@ $<

build/%.o: %.cpp | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/test_runner: $(TEST_OBJ) $(PURE_OBJ) $(VENDOR_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/render_preview: build/render_preview.o $(PURE_OBJ) $(VENDOR_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf build out
EOF
```

- [ ] **Step 2: Run the empty test suite**

Run: `make -C tests test`
Expected: `0 tests, 0 failures`, exit 0.

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: scaffold sketch config, readings and host test harness"
```

---

### Task 2: text_format (fixed-point numbers, uptime)

**Files:**
- Create: `iaq-mk2/text_format.h`, `iaq-mk2/text_format.cpp`
- Test: `tests/test_text_format.cpp`

**Interfaces:**
- Produces: `size_t formatFixed(char* buf, size_t size, float value, uint8_t decimals)`; `size_t formatUptime(char* buf, size_t size, uint32_t seconds)`; `size_t formatUInt(char* buf, size_t size, uint32_t value, uint8_t minDigits)`.

- [ ] **Step 1: Write the failing tests**

```bash
cat > tests/test_text_format.cpp <<'EOF'
#include <cmath>
#include "test.h"
#include "text_format.h"

TEST(formatFixed_rounds_and_pads) {
  char b[16];
  formatFixed(b, sizeof b, 22.56f, 1);  CHECK_STR_EQ(b, "22.6");
  formatFixed(b, sizeof b, 1012.34f, 1); CHECK_STR_EQ(b, "1012.3");
  formatFixed(b, sizeof b, 46.0f, 0);   CHECK_STR_EQ(b, "46");
  formatFixed(b, sizeof b, 0.05f, 1);   CHECK_STR_EQ(b, "0.1");
  formatFixed(b, sizeof b, 86.249f, 2); CHECK_STR_EQ(b, "86.25");
  formatFixed(b, sizeof b, 5.0f, 3);    CHECK_STR_EQ(b, "5.000");
}

TEST(formatFixed_negative_nan_inf) {
  char b[16];
  formatFixed(b, sizeof b, -3.5f, 1);    CHECK_STR_EQ(b, "-3.5");
  formatFixed(b, sizeof b, -0.04f, 1);   CHECK_STR_EQ(b, "0.0");   // rounds to zero: no minus sign
  formatFixed(b, sizeof b, NAN, 1);      CHECK_STR_EQ(b, "--");
  formatFixed(b, sizeof b, INFINITY, 0); CHECK_STR_EQ(b, "--");
}

TEST(formatFixed_truncates_to_buffer) {
  char b[4];
  const size_t n = formatFixed(b, sizeof b, 1234.5f, 1);
  CHECK_EQ(n, 3);
  CHECK_STR_EQ(b, "123");
  CHECK_EQ(formatFixed(b, 0, 1.0f, 0), 0);
}

TEST(formatUptime_units) {
  char b[16];
  formatUptime(b, sizeof b, 0);              CHECK_STR_EQ(b, "0m");
  formatUptime(b, sizeof b, 59);             CHECK_STR_EQ(b, "0m");
  formatUptime(b, sizeof b, 12 * 60);        CHECK_STR_EQ(b, "12m");
  formatUptime(b, sizeof b, 3 * 3600 + 720); CHECK_STR_EQ(b, "3h12m");
  formatUptime(b, sizeof b, 3 * 3600 + 60);  CHECK_STR_EQ(b, "3h01m");
  formatUptime(b, sizeof b, 2 * 86400 + 3 * 3600); CHECK_STR_EQ(b, "2d03h");
}

TEST(formatUInt_min_digits) {
  char b[16];
  formatUInt(b, sizeof b, 7, 2);     CHECK_STR_EQ(b, "07");
  formatUInt(b, sizeof b, 0, 1);     CHECK_STR_EQ(b, "0");
  formatUInt(b, sizeof b, 65000, 0); CHECK_STR_EQ(b, "65000");
}
EOF
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make -C tests test`
Expected: compile error `text_format.h: No such file or directory`.

- [ ] **Step 3: Implement**

```bash
cat > iaq-mk2/text_format.h <<'EOF'
#pragma once
#include <stddef.h>
#include <stdint.h>

// Small, printf-free number formatting shared by the dashboard and the serial log.
// All functions NUL-terminate, truncate to `size - 1` characters and return the
// number of characters written (0 when size == 0).

// Unsigned integer, left-padded with zeros to at least `minDigits` digits.
size_t formatUInt(char* buf, size_t size, uint32_t value, uint8_t minDigits);

// `value` with `decimals` (0..3) fractional digits, rounded half away from zero.
// NaN and infinity format as "--". A value that rounds to zero has no minus sign.
size_t formatFixed(char* buf, size_t size, float value, uint8_t decimals);

// Seconds as "12m", "3h12m" or "2d03h".
size_t formatUptime(char* buf, size_t size, uint32_t seconds);
EOF
cat > iaq-mk2/text_format.cpp <<'EOF'
#include "text_format.h"

#include <math.h>

namespace {

// Appends `s` to buf[*pos..] honouring the size limit. Returns false when truncated.
bool append(char* buf, size_t size, size_t* pos, const char* s) {
  for (; *s; ++s) {
    if (*pos + 1 >= size) {
      buf[size - 1] = '\0';
      return false;
    }
    buf[(*pos)++] = *s;
  }
  buf[*pos] = '\0';
  return true;
}

bool appendUInt(char* buf, size_t size, size_t* pos, uint32_t value, uint8_t minDigits) {
  char digits[11];
  uint8_t n = 0;
  do {
    digits[n++] = (char)('0' + value % 10);
    value /= 10;
  } while (value != 0);
  while (n < minDigits && n < sizeof digits) digits[n++] = '0';
  char out[12];
  uint8_t o = 0;
  while (n > 0) out[o++] = digits[--n];
  out[o] = '\0';
  return append(buf, size, pos, out);
}

}  // namespace

size_t formatUInt(char* buf, size_t size, uint32_t value, uint8_t minDigits) {
  if (size == 0) return 0;
  size_t pos = 0;
  buf[0] = '\0';
  appendUInt(buf, size, &pos, value, minDigits);
  return pos;
}

size_t formatFixed(char* buf, size_t size, float value, uint8_t decimals) {
  if (size == 0) return 0;
  size_t pos = 0;
  buf[0] = '\0';
  if (isnan(value) || isinf(value)) {
    append(buf, size, &pos, "--");
    return pos;
  }
  if (decimals > 3) decimals = 3;
  static const uint32_t kPow10[] = {1, 10, 100, 1000};
  const uint32_t scale = kPow10[decimals];
  const bool negative = value < 0.0f;
  const float magnitude = negative ? -value : value;
  if (magnitude >= 2.0e9f / (float)scale) {
    append(buf, size, &pos, "--");
    return pos;
  }
  const uint32_t scaled = (uint32_t)(magnitude * (float)scale + 0.5f);
  if (negative && scaled != 0) append(buf, size, &pos, "-");
  appendUInt(buf, size, &pos, scaled / scale, 1);
  if (decimals > 0) {
    append(buf, size, &pos, ".");
    appendUInt(buf, size, &pos, scaled % scale, decimals);
  }
  return pos;
}

size_t formatUptime(char* buf, size_t size, uint32_t seconds) {
  if (size == 0) return 0;
  size_t pos = 0;
  buf[0] = '\0';
  const uint32_t minutes = seconds / 60;
  const uint32_t hours = minutes / 60;
  const uint32_t days = hours / 24;
  if (days > 0) {
    appendUInt(buf, size, &pos, days, 1);
    append(buf, size, &pos, "d");
    appendUInt(buf, size, &pos, hours % 24, 2);
    append(buf, size, &pos, "h");
  } else if (hours > 0) {
    appendUInt(buf, size, &pos, hours, 1);
    append(buf, size, &pos, "h");
    appendUInt(buf, size, &pos, minutes % 60, 2);
    append(buf, size, &pos, "m");
  } else {
    appendUInt(buf, size, &pos, minutes, 1);
    append(buf, size, &pos, "m");
  }
  return pos;
}
EOF
```

- [ ] **Step 4: Run tests**

Run: `make -C tests test`
Expected: 5 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: add printf-free fixed-point and uptime formatting"
```

---

### Task 3: air_status (levels and overall status)

**Files:**
- Create: `iaq-mk2/air_status.h`, `iaq-mk2/air_status.cpp`
- Test: `tests/test_air_status.cpp`

**Interfaces:**
- Consumes: `Readings`, `SensorState`, `cfg::*` thresholds.
- Produces: `enum class Level { Unknown, Good, Warn, Bad }`; `Level co2Level(const Co2Reading&)`, `Level pm25Level(const PmReading&)`, `Level aqiLevel(const EnsReading&)`, `Level overallLevel(const Readings&)`, `const char* levelName(Level)` ("WARMUP", "GOOD", "WARN", "BAD").

- [ ] **Step 1: Write the failing tests**

```bash
cat > tests/test_air_status.cpp <<'EOF'
#include "test.h"
#include "air_status.h"

static Co2Reading co2(uint16_t ppm, SensorState s = SensorState::Ok) { Co2Reading r; r.ppm = ppm; r.state = s; return r; }
static PmReading pm25(uint16_t v, SensorState s = SensorState::Ok) { PmReading r; r.pm25 = v; r.state = s; return r; }
static EnsReading aqi(uint8_t v, SensorState s = SensorState::Ok) { EnsReading r; r.aqi = v; r.state = s; return r; }

TEST(co2_thresholds) {
  CHECK(co2Level(co2(799)) == Level::Good);
  CHECK(co2Level(co2(800)) == Level::Warn);
  CHECK(co2Level(co2(1199)) == Level::Warn);
  CHECK(co2Level(co2(1200)) == Level::Bad);
  CHECK(co2Level(co2(5000)) == Level::Bad);
}

TEST(pm25_thresholds) {
  CHECK(pm25Level(pm25(0)) == Level::Good);
  CHECK(pm25Level(pm25(15)) == Level::Good);
  CHECK(pm25Level(pm25(16)) == Level::Warn);
  CHECK(pm25Level(pm25(35)) == Level::Warn);
  CHECK(pm25Level(pm25(36)) == Level::Bad);
}

TEST(aqi_thresholds) {
  CHECK(aqiLevel(aqi(1)) == Level::Good);
  CHECK(aqiLevel(aqi(2)) == Level::Warn);
  CHECK(aqiLevel(aqi(3)) == Level::Bad);
  CHECK(aqiLevel(aqi(5)) == Level::Bad);
}

TEST(non_ok_states_are_unknown) {
  CHECK(co2Level(co2(2000, SensorState::WarmingUp)) == Level::Unknown);
  CHECK(co2Level(co2(2000, SensorState::Stale)) == Level::Unknown);
  CHECK(pm25Level(pm25(100, SensorState::NotReady)) == Level::Unknown);
  CHECK(aqiLevel(aqi(5, SensorState::Unavailable)) == Level::Unknown);
}

TEST(overall_is_worst_of_valid_metrics) {
  Readings r;
  CHECK(overallLevel(r) == Level::Unknown);           // nothing valid yet
  r.pm = pm25(5);
  CHECK(overallLevel(r) == Level::Good);              // CO2 still warming up, PM is fine
  r.co2 = co2(900);
  CHECK(overallLevel(r) == Level::Warn);
  r.ens = aqi(4);
  CHECK(overallLevel(r) == Level::Bad);
  r.ens.state = SensorState::Stale;                   // stale sensor no longer counts
  CHECK(overallLevel(r) == Level::Warn);
}

TEST(level_names) {
  CHECK_STR_EQ(levelName(Level::Unknown), "WARMUP");
  CHECK_STR_EQ(levelName(Level::Good), "GOOD");
  CHECK_STR_EQ(levelName(Level::Warn), "WARN");
  CHECK_STR_EQ(levelName(Level::Bad), "BAD");
}
EOF
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make -C tests test`
Expected: compile error `air_status.h: No such file or directory`.

- [ ] **Step 3: Implement**

```bash
cat > iaq-mk2/air_status.h <<'EOF'
#pragma once
#include <stdint.h>
#include "readings.h"

// Pure classification of readings into traffic-light levels. Only readings in
// SensorState::Ok take part; everything else is Unknown.
enum class Level : uint8_t { Unknown = 0, Good, Warn, Bad };

Level co2Level(const Co2Reading& r);
Level pm25Level(const PmReading& r);
Level aqiLevel(const EnsReading& r);

// Worst level among the valid metrics; Unknown when none is valid.
Level overallLevel(const Readings& r);

const char* levelName(Level level);  // "WARMUP", "GOOD", "WARN", "BAD"
EOF
cat > iaq-mk2/air_status.cpp <<'EOF'
#include "air_status.h"

#include "config.h"

Level co2Level(const Co2Reading& r) {
  if (r.state != SensorState::Ok) return Level::Unknown;
  if (r.ppm >= cfg::CO2_BAD_PPM) return Level::Bad;
  if (r.ppm >= cfg::CO2_WARN_PPM) return Level::Warn;
  return Level::Good;
}

Level pm25Level(const PmReading& r) {
  if (r.state != SensorState::Ok) return Level::Unknown;
  if (r.pm25 >= cfg::PM25_BAD_UGM3) return Level::Bad;
  if (r.pm25 >= cfg::PM25_WARN_UGM3) return Level::Warn;
  return Level::Good;
}

Level aqiLevel(const EnsReading& r) {
  if (r.state != SensorState::Ok) return Level::Unknown;
  if (r.aqi >= cfg::AQI_BAD) return Level::Bad;
  if (r.aqi >= cfg::AQI_WARN) return Level::Warn;
  return Level::Good;
}

Level overallLevel(const Readings& r) {
  const Level levels[] = {co2Level(r.co2), pm25Level(r.pm), aqiLevel(r.ens)};
  Level worst = Level::Unknown;
  for (const Level l : levels) {
    if (l == Level::Unknown) continue;
    if (worst == Level::Unknown || l > worst) worst = l;
  }
  return worst;
}

const char* levelName(Level level) {
  switch (level) {
    case Level::Good: return "GOOD";
    case Level::Warn: return "WARN";
    case Level::Bad:  return "BAD";
    case Level::Unknown: break;
  }
  return "WARMUP";
}
EOF
```

- [ ] **Step 4: Run tests**

Run: `make -C tests test`
Expected: 11 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: add air quality level classification"
```

---

### Task 4: PMS7003 frame parser

**Files:**
- Create: `iaq-mk2/pms7003.h`, `iaq-mk2/pms7003.cpp`
- Test: `tests/test_pms7003.cpp`

**Interfaces:**
- Produces: `struct PmsFrame { uint16_t pm1Std, pm25Std, pm10Std, pm1Atm, pm25Atm, pm10Atm; }`; `class Pms7003Parser { bool feed(uint8_t byte, PmsFrame& out); uint32_t framesOk() const; uint32_t checksumErrors() const; uint32_t lengthErrors() const; }`.

- [ ] **Step 1: Write the failing tests**

```bash
cat > tests/test_pms7003.cpp <<'EOF'
#include <vector>
#include "test.h"
#include "pms7003.h"

// Builds a valid 32-byte PMS7003 frame with the given atmospheric PM values.
static std::vector<uint8_t> makeFrame(uint16_t pm1Atm, uint16_t pm25Atm, uint16_t pm10Atm) {
  std::vector<uint8_t> f(32, 0);
  f[0] = 0x42; f[1] = 0x4D; f[2] = 0x00; f[3] = 28;
  f[4] = 0; f[5] = 1; f[6] = 0; f[7] = 2; f[8] = 0; f[9] = 3;   // CF=1 values 1,2,3
  f[10] = pm1Atm >> 8; f[11] = pm1Atm & 0xFF;
  f[12] = pm25Atm >> 8; f[13] = pm25Atm & 0xFF;
  f[14] = pm10Atm >> 8; f[15] = pm10Atm & 0xFF;
  uint16_t sum = 0;
  for (int i = 0; i < 30; ++i) sum += f[i];
  f[30] = sum >> 8; f[31] = sum & 0xFF;
  return f;
}

static int feedAll(Pms7003Parser& p, const std::vector<uint8_t>& bytes, PmsFrame& out) {
  int frames = 0;
  for (uint8_t b : bytes) if (p.feed(b, out)) ++frames;
  return frames;
}

TEST(pms_parses_valid_frame) {
  Pms7003Parser p; PmsFrame f{};
  CHECK_EQ(feedAll(p, makeFrame(5, 8, 11), f), 1);
  CHECK_EQ(f.pm1Atm, 5); CHECK_EQ(f.pm25Atm, 8); CHECK_EQ(f.pm10Atm, 11);
  CHECK_EQ(f.pm1Std, 1); CHECK_EQ(f.pm25Std, 2); CHECK_EQ(f.pm10Std, 3);
  CHECK_EQ(p.framesOk(), 1);
}

TEST(pms_resyncs_after_noise_and_partial_frame) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> stream = {0x00, 0xFF, 0x4D, 0x42, 0x11};        // noise, lone 0x4D, false start
  std::vector<uint8_t> partial = makeFrame(1, 1, 1);
  partial.resize(20);                                                   // truncated frame
  stream.insert(stream.end(), partial.begin(), partial.end());
  std::vector<uint8_t> good = makeFrame(7, 9, 12);
  stream.insert(stream.end(), good.begin(), good.end());
  CHECK_EQ(feedAll(p, stream, f), 1);
  CHECK_EQ(f.pm25Atm, 9);
}

TEST(pms_double_start_byte_does_not_lose_frame) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> stream = {0x42};                                 // stray start byte
  std::vector<uint8_t> good = makeFrame(3, 4, 5);
  stream.insert(stream.end(), good.begin(), good.end());
  CHECK_EQ(feedAll(p, stream, f), 1);
  CHECK_EQ(f.pm25Atm, 4);
}

TEST(pms_rejects_bad_checksum_and_counts_it) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> bad = makeFrame(3, 4, 5);
  bad[13] ^= 0x01;                                                      // corrupt payload
  CHECK_EQ(feedAll(p, bad, f), 0);
  CHECK_EQ(p.checksumErrors(), 1);
  CHECK_EQ(feedAll(p, makeFrame(1, 2, 3), f), 1);                       // recovers immediately
  CHECK_EQ(f.pm25Atm, 2);
}

TEST(pms_rejects_wrong_length_field) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> bad = makeFrame(3, 4, 5);
  bad[3] = 20;
  std::vector<uint8_t> stream = bad;
  std::vector<uint8_t> good = makeFrame(6, 7, 8);
  stream.insert(stream.end(), good.begin(), good.end());
  CHECK_EQ(feedAll(p, stream, f), 1);
  CHECK_EQ(p.lengthErrors(), 1);
  CHECK_EQ(f.pm25Atm, 7);
}

TEST(pms_back_to_back_frames) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> stream;
  for (int i = 0; i < 5; ++i) { auto g = makeFrame(i, i * 2, i * 3); stream.insert(stream.end(), g.begin(), g.end()); }
  CHECK_EQ(feedAll(p, stream, f), 5);
  CHECK_EQ(f.pm10Atm, 12);
}
EOF
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make -C tests test`
Expected: compile error `pms7003.h: No such file or directory`.

- [ ] **Step 3: Implement**

```bash
cat > iaq-mk2/pms7003.h <<'EOF'
#pragma once
#include <stdint.h>

// Incremental parser for Plantower PMS7003 32-byte data frames:
//   0x42 0x4D, length (28), 6x PM (CF=1 then atmospheric), 6x particle counts,
//   reserved, 16-bit checksum = sum of bytes 0..29. All values big-endian.
// Feed one byte at a time from the UART; `feed` returns true exactly when a
// checksum-valid frame has been completed and copied into `out`.

struct PmsFrame {
  uint16_t pm1Std = 0;   // CF=1 ("standard particle") values
  uint16_t pm25Std = 0;
  uint16_t pm10Std = 0;
  uint16_t pm1Atm = 0;   // atmospheric environment values (use these)
  uint16_t pm25Atm = 0;
  uint16_t pm10Atm = 0;
};

class Pms7003Parser {
 public:
  static constexpr uint8_t kFrameLength = 32;

  bool feed(uint8_t byte, PmsFrame& out);

  uint32_t framesOk() const { return framesOk_; }
  uint32_t checksumErrors() const { return checksumErrors_; }
  uint32_t lengthErrors() const { return lengthErrors_; }

 private:
  static constexpr uint8_t kStart1 = 0x42;
  static constexpr uint8_t kStart2 = 0x4D;
  static constexpr uint16_t kPayloadLength = 28;

  void restart(uint8_t byte);
  void resyncFromTail(PmsFrame& out);

  uint8_t buf_[kFrameLength] = {};
  uint8_t idx_ = 0;
  uint32_t framesOk_ = 0;
  uint32_t checksumErrors_ = 0;
  uint32_t lengthErrors_ = 0;
};
EOF
cat > iaq-mk2/pms7003.cpp <<'EOF'
#include "pms7003.h"

namespace {
uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
}  // namespace

// Drop the current frame; if `byte` looks like a start byte, begin a new one with it.
void Pms7003Parser::restart(uint8_t byte) {
  idx_ = 0;
  if (byte == kStart1) buf_[idx_++] = byte;
}

// A checksum failure usually means bytes were lost and the buffer already holds
// the start of the next frame. Re-feed bytes 1..31 so that frame is not lost.
// At most 31 bytes are re-fed, so this cannot complete a frame or recurse.
void Pms7003Parser::resyncFromTail(PmsFrame& out) {
  uint8_t tail[kFrameLength - 1];
  for (uint8_t i = 0; i < kFrameLength - 1; ++i) tail[i] = buf_[i + 1];
  idx_ = 0;
  for (uint8_t i = 0; i < kFrameLength - 1; ++i) feed(tail[i], out);
}

bool Pms7003Parser::feed(uint8_t byte, PmsFrame& out) {
  if (idx_ == 0) {
    if (byte == kStart1) buf_[idx_++] = byte;
    return false;
  }
  if (idx_ == 1) {
    if (byte == kStart2) {
      buf_[idx_++] = byte;
    } else if (byte != kStart1) {
      idx_ = 0;  // a repeated 0x42 keeps idx_ == 1
    }
    return false;
  }

  buf_[idx_++] = byte;

  if (idx_ == 4 && be16(buf_ + 2) != kPayloadLength) {
    ++lengthErrors_;
    restart(byte);
    return false;
  }
  if (idx_ < kFrameLength) return false;

  idx_ = 0;
  uint16_t sum = 0;
  for (uint8_t i = 0; i < kFrameLength - 2; ++i) sum += buf_[i];
  if (sum != be16(buf_ + 30)) {
    ++checksumErrors_;
    resyncFromTail(out);
    return false;
  }

  out.pm1Std = be16(buf_ + 4);
  out.pm25Std = be16(buf_ + 6);
  out.pm10Std = be16(buf_ + 8);
  out.pm1Atm = be16(buf_ + 10);
  out.pm25Atm = be16(buf_ + 12);
  out.pm10Atm = be16(buf_ + 14);
  ++framesOk_;
  return true;
}
EOF
```

- [ ] **Step 4: Run tests**

Run: `make -C tests test`
Expected: 17 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: add PMS7003 frame parser"
```

---

### Task 5: SEN0219 PWM conversion

**Files:**
- Create: `iaq-mk2/sen0219.h`, `iaq-mk2/sen0219.cpp`
- Test: `tests/test_sen0219.cpp`

**Interfaces:**
- Produces: `struct Co2Result { bool valid; uint16_t ppm; }`; `Co2Result co2FromPulse(uint32_t highUs, uint32_t lowUs)`; `constexpr uint32_t kCo2PeriodUs = 1004000`.

- [ ] **Step 1: Write the failing tests**

```bash
cat > tests/test_sen0219.cpp <<'EOF'
#include "test.h"
#include "sen0219.h"

TEST(co2_datasheet_formula) {
  // ppm = 5000 * (TH - 2 ms) / (TH + TL - 4 ms), cycle 1004 ms
  Co2Result r = co2FromPulse(2000 + 400 * 200, 1004000 - (2000 + 400 * 200));
  CHECK(r.valid);
  CHECK_EQ(r.ppm, 400);
  r = co2FromPulse(2000 + 1000 * 200, 1004000 - (2000 + 1000 * 200));
  CHECK(r.valid);
  CHECK_EQ(r.ppm, 1000);
}

TEST(co2_endpoints) {
  CHECK_EQ(co2FromPulse(2000, 1002000).ppm, 0);        // TH = 2 ms -> 0 ppm
  CHECK_EQ(co2FromPulse(1002000, 2000).ppm, 5000);     // TL = 2 ms -> 5000 ppm
  CHECK(co2FromPulse(1002000, 2000).valid);
}

TEST(co2_tolerates_period_jitter) {
  // Same duty at a 4 % slow cycle still converts
  const uint32_t period = 1044000;
  const uint32_t high = 2000 + (uint32_t)((period - 4000) * 0.16);     // ~800 ppm
  Co2Result r = co2FromPulse(high, period - high);
  CHECK(r.valid);
  CHECK(r.ppm >= 795 && r.ppm <= 805);
}

TEST(co2_rejects_implausible_cycles) {
  CHECK(!co2FromPulse(100000, 100000).valid);          // 200 ms period
  CHECK(!co2FromPulse(1000000, 1000000).valid);        // 2 s period
  CHECK(!co2FromPulse(0, 1004000).valid);              // no high pulse
  CHECK(!co2FromPulse(1500, 1002500).valid);           // TH < 2 ms lead pulse
  CHECK(!co2FromPulse(1004000, 0).valid);              // stuck high
}

TEST(co2_wraparound_safe_inputs) {
  // Callers compute durations with unsigned subtraction; verify a duration that
  // crossed the micros() wrap still converts when passed as plain durations.
  const uint32_t riseUs = 0xFFFFFF00u, fallUs = riseUs + 202000u;  // wraps
  const uint32_t highUs = fallUs - riseUs;
  CHECK_EQ(highUs, 202000u);
  CHECK(co2FromPulse(highUs, 1004000 - highUs).valid);
}
EOF
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make -C tests test`
Expected: compile error `sen0219.h: No such file or directory`.

- [ ] **Step 3: Implement**

```bash
cat > iaq-mk2/sen0219.h <<'EOF'
#pragma once
#include <stdint.h>

// DFRobot SEN0219 (Winsen MH-Z14-class NDIR) PWM output:
//   cycle 1004 ms +-5 %, 2 ms high at the start and 2 ms low at the end,
//   ppm = 5000 * (TH - 2 ms) / (TH + TL - 4 ms).
// Pure conversion from one measured cycle to ppm with plausibility checks.

constexpr uint32_t kCo2PeriodUs = 1004000;
constexpr uint16_t kCo2RangePpm = 5000;

struct Co2Result {
  bool valid;
  uint16_t ppm;
};

Co2Result co2FromPulse(uint32_t highUs, uint32_t lowUs);
EOF
cat > iaq-mk2/sen0219.cpp <<'EOF'
#include "sen0219.h"

namespace {
constexpr uint32_t kLeadTrailUs = 2000;                           // 2 ms high lead + 2 ms low trail
constexpr uint32_t kMinPeriodUs = kCo2PeriodUs / 10 * 9;          // -10 %
constexpr uint32_t kMaxPeriodUs = kCo2PeriodUs / 10 * 11;         // +10 %
}  // namespace

Co2Result co2FromPulse(uint32_t highUs, uint32_t lowUs) {
  const uint32_t periodUs = highUs + lowUs;
  if (periodUs < kMinPeriodUs || periodUs > kMaxPeriodUs) return {false, 0};
  if (highUs < kLeadTrailUs || lowUs < kLeadTrailUs) return {false, 0};

  // Integer arithmetic: numerator fits in 64 bits comfortably.
  const uint64_t numerator = (uint64_t)kCo2RangePpm * (highUs - kLeadTrailUs);
  const uint32_t denominator = periodUs - 2 * kLeadTrailUs;
  const uint32_t ppm = (uint32_t)((numerator + denominator / 2) / denominator);
  if (ppm > kCo2RangePpm) return {false, 0};
  return {true, (uint16_t)ppm};
}
EOF
```

- [ ] **Step 4: Run tests**

Run: `make -C tests test`
Expected: 22 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: add SEN0219 PWM to ppm conversion"
```

---

### Task 6: Vendor the Waveshare driver with RP2040 DEV_Config and non-blocking patch

**Files:**
- Create: `iaq-mk2/src/waveshare/{EPD_2in9g.h,EPD_2in9g.cpp,GUI_Paint.h,GUI_Paint.cpp,fonts.h,font8.cpp,font12.cpp,font16.cpp,font20.cpp,font24.cpp,DEV_Config.h,DEV_Config.cpp,Debug.h,README.md}`
- Create: `iaq-mk2/watchdog.h`, `iaq-mk2/watchdog.cpp` (DEV_Config kicks it)

**Interfaces:**
- Produces: vendor API `EPD_2IN9G_Init()`, `EPD_2IN9G_Init_Fast()`, `EPD_2IN9G_Display(const UBYTE*)`, `EPD_2IN9G_Sleep()`, plus patch additions `void EPD_2IN9G_Display_Start(const UBYTE* Image)` (send + trigger, no wait) and `UBYTE EPD_2IN9G_IsBusy(void)` (1 while BUSY is low). `DEV_Module_Init()`, `DEV_Delay_ms(uint32_t)`, `DEV_SPI_WriteByte(UBYTE)`. `namespace watchdog { void begin(uint32_t ms); void kick(); bool active(); }`.

- [ ] **Step 1: Copy the vendor files from the Waveshare demo**

Source: `https://files.waveshare.com/wiki/2.9inch_e-Paper_Module_G/2.9inch_e-Paper_G.zip`, folder `2.9inch_e-Paper_G/Arduino_R4/`, driver dated 2025-12-02.

```bash
SRC=/path/to/unzipped/2.9inch_e-Paper_G/Arduino_R4   # adjust
DST=iaq-mk2/src/waveshare
cp "$SRC"/EPD_2in9g.h "$SRC"/EPD_2in9g.cpp "$SRC"/GUI_Paint.h "$SRC"/GUI_Paint.cpp "$SRC"/fonts.h \
   "$SRC"/font8.cpp "$SRC"/font12.cpp "$SRC"/font16.cpp "$SRC"/font20.cpp "$SRC"/font24.cpp "$DST"/
```

- [ ] **Step 2: Write our DEV_Config, Debug.h, watchdog and vendor README**

```bash
cat > iaq-mk2/src/waveshare/Debug.h <<'EOF'
// IAQ-MK2 replacement for Waveshare's Debug.h: the vendor version prints
// "e-Paper busy" lines inside polling loops and pulls in Wire.h. Logging is
// done by the application, so the vendor macro is a no-op here.
#ifndef __DEBUG_H
#define __DEBUG_H
#define Debug(__info) ((void)0)
#endif
EOF
cat > iaq-mk2/src/waveshare/DEV_Config.h <<'EOF'
// IAQ-MK2 replacement for Waveshare's DEV_Config.h (hardware abstraction used by
// EPD_2in9g.cpp and GUI_Paint.cpp). Pins come from config.h. On the host build
// (unit tests) only the type aliases are needed.
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#ifndef IAQ_HOST_BUILD
#include <Arduino.h>
#include <SPI.h>
#include "../../config.h"

#define EPD_CS_PIN    cfg::PIN_EPD_CS
#define EPD_DC_PIN    cfg::PIN_EPD_DC
#define EPD_RST_PIN   cfg::PIN_EPD_RST
#define EPD_BUSY_PIN  cfg::PIN_EPD_BUSY
#define EPD_PWR_PIN   cfg::PIN_EPD_PWR
#define EPD_BUSY_TIMEOUT_MS cfg::EPD_BUSY_TIMEOUT_MS

#define GPIO_PIN_SET   1
#define GPIO_PIN_RESET 0

#define DEV_Digital_Write(_pin, _value) digitalWrite(_pin, (_value) == 0 ? LOW : HIGH)
#define DEV_Digital_Read(_pin) digitalRead(_pin)

// Delays in slices and kicks the watchdog so vendor busy loops cannot trip it.
void DEV_Delay_ms(uint32_t ms);

UBYTE DEV_Module_Init(void);
void DEV_SPI_WriteByte(UBYTE data);
void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len);
#endif  // IAQ_HOST_BUILD

#endif
EOF
cat > iaq-mk2/src/waveshare/DEV_Config.cpp <<'EOF'
// IAQ-MK2 replacement for Waveshare's DEV_Config.cpp targeting the Arduino
// Nano RP2040 Connect (mbed core): hardware SPI0 on D11/D13, control pins
// from config.h, PWR pin driven high, BUSY with pull-up so a missing panel reads
// "idle" instead of hanging the busy wait.
#include "DEV_Config.h"

#include "../../watchdog.h"

UBYTE DEV_Module_Init(void) {
  pinMode(EPD_BUSY_PIN, INPUT_PULLUP);
  pinMode(EPD_RST_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_PWR_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);

  digitalWrite(EPD_PWR_PIN, HIGH);
  digitalWrite(EPD_CS_PIN, HIGH);
  digitalWrite(EPD_RST_PIN, HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  return 0;
}

void DEV_Delay_ms(uint32_t ms) {
  while (ms > 0) {
    const uint32_t step = ms > 50 ? 50 : ms;
    delay(step);
    ms -= step;
    watchdog::kick();
  }
}

void DEV_SPI_WriteByte(UBYTE data) { SPI.transfer(data); }

void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len) {
  for (UDOUBLE i = 0; i < len; i++) DEV_SPI_WriteByte(pData[i]);
}
EOF
cat > iaq-mk2/watchdog.h <<'EOF'
#pragma once
#include <stdint.h>

// RP2040 hardware watchdog (mbed driver). A hang anywhere reboots the board.
namespace watchdog {
void begin(uint32_t timeoutMs);  // clamped to the hardware maximum (~8.3 s)
void kick();
bool active();
}  // namespace watchdog
EOF
cat > iaq-mk2/watchdog.cpp <<'EOF'
#include "watchdog.h"

#include <Arduino.h>
#include <mbed.h>

namespace {
bool g_started = false;
}

void watchdog::begin(uint32_t timeoutMs) {
  mbed::Watchdog& wd = mbed::Watchdog::get_instance();
  const uint32_t maxMs = wd.get_max_timeout();
  g_started = wd.start(timeoutMs > maxMs ? maxMs : timeoutMs);
}

void watchdog::kick() {
  if (g_started) mbed::Watchdog::get_instance().kick();
}

bool watchdog::active() { return g_started; }
EOF
cat > iaq-mk2/src/waveshare/README.md <<'EOF'
# Vendored Waveshare e-Paper driver

Source: `2.9inch_e-Paper_G.zip` from the Waveshare wiki
(https://www.waveshare.com/wiki/2.9inch_e-Paper_Module_(G)), folder
`Arduino_R4`, driver version dated 2025-12-02. MIT licensed (see file headers).

Unmodified: `GUI_Paint.{h,cpp}`, `fonts.h`, `font8/12/16/20/24.cpp`.

Replaced: `DEV_Config.{h,cpp}` (RP2040 pins from `config.h`, PWR pin, watchdog
kick in delays, BUSY pull-up), `Debug.h` (no-op).

Patched `EPD_2in9g.{h,cpp}` (search for `IAQ-MK2`):
- busy wait has a timeout (`EPD_BUSY_TIMEOUT_MS`) instead of spinning forever;
- `EPD_2IN9G_Display_Start()` sends the frame and triggers the refresh without waiting;
- `EPD_2IN9G_IsBusy()` exposes the BUSY pin so the application can poll.

Known vendor quirks the application works around:
- `Paint_DrawString_EN(..., fg, bg)` calls `Paint_DrawChar(..., bg, fg)`: the
  arguments are effectively swapped. Use `Paint_DrawChar` directly.
- `Paint_DrawPoint`/`Paint_DrawLine` draw one pixel up-left of the requested
  coordinate; `Paint_SetPixel` bounds check is `>` instead of `>=`.
EOF
```

- [ ] **Step 3: Apply the EPD_2in9g patch**

```bash
python3 - <<'EOF'
import re, pathlib
p = pathlib.Path("iaq-mk2/src/waveshare/EPD_2in9g.cpp")
s = p.read_text()

old_busy = """static void EPD_2IN9G_ReadBusyH(void)
{
    Debug("e-Paper busy H\\r\\n");
    DEV_Delay_ms(100);
    while(!DEV_Digital_Read(EPD_BUSY_PIN)) {      //LOW: idle, HIGH: busy
        DEV_Delay_ms(5);
    }
    Debug("e-Paper busy H release\\r\\n");
}"""
new_busy = """static void EPD_2IN9G_ReadBusyH(void)
{
    // IAQ-MK2 patch: bounded wait. BUSY is LOW while the panel is busy.
    DEV_Delay_ms(100);
    UDOUBLE waited_ms = 100;
    while(!DEV_Digital_Read(EPD_BUSY_PIN)) {
        DEV_Delay_ms(5);
        waited_ms += 5;
        if (waited_ms > EPD_BUSY_TIMEOUT_MS) {
            return;
        }
    }
}"""
assert old_busy in s, "ReadBusyH anchor not found"
s = s.replace(old_busy, new_busy)

old_sleep = """/******************************************************************************
function :	Enter sleep mode"""
new_sleep = """/******************************************************************************
IAQ-MK2 patch: non-blocking refresh support.
EPD_2IN9G_Display_Start sends the frame and issues DISPLAY_REFRESH without
waiting; the application polls EPD_2IN9G_IsBusy() and then calls
EPD_2IN9G_Sleep().
******************************************************************************/
void EPD_2IN9G_Display_Start(const UBYTE *Image)
{
    UWORD Width, Height;
    Width = (EPD_2IN9G_WIDTH % 4 == 0)? (EPD_2IN9G_WIDTH / 4 ): (EPD_2IN9G_WIDTH / 4 + 1);
    Height = EPD_2IN9G_HEIGHT;

    EPD_2IN9G_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_2IN9G_SendData(Image[i + j * Width]);
        }
    }

    EPD_2IN9G_SendCommand(0x12); // DISPLAY_REFRESH
    EPD_2IN9G_SendData(0x00);
}

UBYTE EPD_2IN9G_IsBusy(void)
{
    return DEV_Digital_Read(EPD_BUSY_PIN) ? 0 : 1;   // LOW = busy
}

/******************************************************************************
function :	Enter sleep mode"""
assert old_sleep in s, "Sleep anchor not found"
s = s.replace(old_sleep, new_sleep, 1)
p.write_text(s)

h = pathlib.Path("iaq-mk2/src/waveshare/EPD_2in9g.h")
t = h.read_text()
old = "void EPD_2IN9G_Sleep(void);"
new = """void EPD_2IN9G_Sleep(void);

// IAQ-MK2 patch: non-blocking refresh support (see EPD_2in9g.cpp).
void EPD_2IN9G_Display_Start(const UBYTE *Image);
UBYTE EPD_2IN9G_IsBusy(void);"""
assert old in t
h.write_text(t.replace(old, new, 1))
print("patched")
EOF
```

- [ ] **Step 4: Verify the vendor Paint library compiles on the host**

Run: `make -C tests test`
Expected: vendor objects build (with `-w`), 22 tests, 0 failures.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "vendor: add Waveshare 2.9inch G driver with RP2040 DEV_Config and non-blocking patch"
```

---

### Task 7: Dashboard renderer with host preview

**Files:**
- Create: `iaq-mk2/dashboard.h`, `iaq-mk2/dashboard.cpp`, `tests/render_preview.cpp`, `tests/ppm_to_png.py`
- Test: `tests/test_dashboard.cpp`

**Interfaces:**
- Consumes: `Readings`, `Level`/`levelName`/`co2Level`/`pm25Level`/`aqiLevel`/`overallLevel`, `formatFixed`, `formatUptime`, vendor Paint API, `EPD_2IN9G_*` colour constants.
- Produces: `constexpr size_t kFrameBufferSize = 9472`; `void dashboardRender(const Readings& r, uint32_t uptimeSeconds, uint8_t* frameBuffer)`; `uint8_t dashboardPixel(const uint8_t* fb, int x, int y)` (landscape pixel decode, used by tests).

- [ ] **Step 1: Write the failing tests**

```bash
cat > tests/test_dashboard.cpp <<'EOF'
#include <cstring>
#include "test.h"
#include "dashboard.h"
#include "readings.h"

namespace {
constexpr size_t kCanary = 64;
struct GuardedBuffer {
  uint8_t bytes[kFrameBufferSize + kCanary];
  GuardedBuffer() { std::memset(bytes, 0xA5, sizeof bytes); }
  bool canaryIntact() const {
    for (size_t i = kFrameBufferSize; i < sizeof bytes; ++i) if (bytes[i] != 0xA5) return false;
    return true;
  }
};

Readings extreme() {
  Readings r;
  r.co2.ppm = 5000; r.co2.state = SensorState::Ok;
  r.pm.pm1 = 999; r.pm.pm25 = 999; r.pm.pm10 = 999; r.pm.state = SensorState::Ok;
  r.ens.aqi = 5; r.ens.tvocPpb = 65000; r.ens.eco2Ppm = 65000; r.ens.state = SensorState::Ok;
  r.env.temperatureC = -10.5f; r.env.humidityPct = 100.0f; r.env.pressureHpa = 1099.9f; r.env.gasKOhm = 9999.0f;
  r.env.state = SensorState::Ok;
  return r;
}

int countColour(const uint8_t* fb, uint8_t colour) {
  int n = 0;
  for (int y = 0; y < 128; ++y) for (int x = 0; x < 296; ++x) if (dashboardPixel(fb, x, y) == colour) ++n;
  return n;
}
}  // namespace

TEST(dashboard_extreme_values_stay_in_bounds) {
  GuardedBuffer g;
  dashboardRender(extreme(), 99 * 86400 + 23 * 3600, g.bytes);
  CHECK(g.canaryIntact());
  CHECK(countColour(g.bytes, 0) > 500);          // black ink present
  CHECK(countColour(g.bytes, 3) > 50);           // red: BAD badge and values
}

TEST(dashboard_good_has_no_warning_colours) {
  Readings r;
  r.co2.ppm = 650; r.co2.state = SensorState::Ok;
  r.pm.pm1 = 3; r.pm.pm25 = 5; r.pm.pm10 = 7; r.pm.state = SensorState::Ok;
  r.ens.aqi = 1; r.ens.tvocPpb = 40; r.ens.state = SensorState::Ok;
  r.env.temperatureC = 22.6f; r.env.humidityPct = 46.0f; r.env.pressureHpa = 1012.3f; r.env.gasKOhm = 86.2f;
  r.env.state = SensorState::Ok;
  GuardedBuffer g;
  dashboardRender(r, 3 * 3600 + 12 * 60, g.bytes);
  CHECK(g.canaryIntact());
  CHECK_EQ(countColour(g.bytes, 2), 0);
  CHECK_EQ(countColour(g.bytes, 3), 0);
}

TEST(dashboard_warmup_renders_without_valid_data) {
  Readings r;                                     // everything NotReady
  r.env.state = SensorState::Unavailable;
  GuardedBuffer g;
  dashboardRender(r, 5, g.bytes);
  CHECK(g.canaryIntact());
  CHECK(countColour(g.bytes, 0) > 500);
}

TEST(dashboard_pixel_decoder_matches_rotation) {
  GuardedBuffer g;
  Readings r;
  dashboardRender(r, 0, g.bytes);
  // Header rule at y = 19 is black across the full width; row 60 near the
  // right edge is white in the warm-up screen.
  CHECK_EQ(dashboardPixel(g.bytes, 0, 19), 0);
  CHECK_EQ(dashboardPixel(g.bytes, 295, 19), 0);
  CHECK_EQ(dashboardPixel(g.bytes, 295, 60), 1);
}
EOF
cat > tests/ppm_to_png.py <<'EOF'
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow>=10"]
# ///
"""Convert every .ppm in a directory to .png (dashboard previews)."""
import sys
from pathlib import Path

from PIL import Image

out = Path(sys.argv[1] if len(sys.argv) > 1 else "out")
for ppm in sorted(out.glob("*.ppm")):
    png = ppm.with_suffix(".png")
    with Image.open(ppm) as img:
        img = img.resize((img.width * 3, img.height * 3), Image.NEAREST)
        img.save(png)
    ppm.unlink()
    print(png)
EOF
cat > tests/render_preview.cpp <<'EOF'
// Renders dashboard scenarios to PPM files so the layout can be checked on the host.
#include <cstdio>
#include <cstring>
#include <string>

#include "dashboard.h"
#include "readings.h"

namespace {
const uint8_t kPalette[4][3] = {{0, 0, 0}, {255, 255, 255}, {250, 200, 0}, {220, 30, 30}};

void writePpm(const std::string& path, const uint8_t* fb) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::perror(path.c_str()); return; }
  std::fprintf(f, "P6\n296 128\n255\n");
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 296; ++x) std::fwrite(kPalette[dashboardPixel(fb, x, y) & 3], 1, 3, f);
  std::fclose(f);
  std::printf("wrote %s\n", path.c_str());
}

Readings good() {
  Readings r;
  r.co2 = {684, SensorState::Ok};
  r.pm = {5, 8, 11, SensorState::Ok};
  r.ens.aqi = 1; r.ens.tvocPpb = 42; r.ens.eco2Ppm = 612; r.ens.state = SensorState::Ok;
  r.env = {22.6f, 46.0f, 1012.3f, 86.2f, SensorState::Ok};
  return r;
}
}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "out";
  static uint8_t fb[kFrameBufferSize];

  Readings r = good();
  dashboardRender(r, 3 * 3600 + 12 * 60, fb);
  writePpm(dir + "/01_good.ppm", fb);

  r = good(); r.co2.ppm = 950; r.pm.pm25 = 22; r.ens.aqi = 2;
  dashboardRender(r, 26 * 3600, fb);
  writePpm(dir + "/02_warn.ppm", fb);

  r = good(); r.co2.ppm = 1850; r.pm.pm25 = 48; r.pm.pm10 = 61; r.ens.aqi = 4; r.ens.tvocPpb = 1200;
  dashboardRender(r, 5 * 86400 + 2 * 3600, fb);
  writePpm(dir + "/03_bad.ppm", fb);

  Readings w;                                   // boot screen
  w.pm.state = SensorState::NotReady;
  w.env = {21.9f, 51.0f, 1008.7f, 12.0f, SensorState::Ok};
  dashboardRender(w, 4, fb);
  writePpm(dir + "/04_boot.ppm", fb);

  r = good(); r.co2.state = SensorState::Stale; r.pm.state = SensorState::WarmingUp;
  r.ens.state = SensorState::WarmingUp; r.ens.validityFlag = 2; r.env.state = SensorState::Unavailable;
  dashboardRender(r, 90, fb);
  writePpm(dir + "/05_degraded.ppm", fb);

  r = good(); r.co2.ppm = 5000; r.pm = {999, 999, 999, SensorState::Ok}; r.ens.aqi = 5; r.ens.tvocPpb = 65000;
  r.env = {-10.5f, 100.0f, 1099.9f, 9999.0f, SensorState::Ok};
  dashboardRender(r, 99 * 86400 + 23 * 3600, fb);
  writePpm(dir + "/06_extreme.ppm", fb);
  return 0;
}
EOF
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make -C tests test`
Expected: compile error `dashboard.h: No such file or directory`.

- [ ] **Step 3: Implement the renderer**

```bash
cat > iaq-mk2/dashboard.h <<'EOF'
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "readings.h"

// Renders the status panel into a 2-bit-per-pixel framebuffer for the
// Waveshare 2.9" (G) panel (128 x 296 memory, shown as 296 x 128 landscape).
// Pure: depends only on the Paint library, no Arduino APIs.

constexpr int kDashboardWidth = 296;
constexpr int kDashboardHeight = 128;
constexpr size_t kFrameBufferSize = (128 / 4) * 296;  // 9472 bytes

void dashboardRender(const Readings& r, uint32_t uptimeSeconds, uint8_t* frameBuffer);

// Colour code (0 black, 1 white, 2 yellow, 3 red) of landscape pixel (x, y)
// in a buffer produced by dashboardRender. For tests and previews.
uint8_t dashboardPixel(const uint8_t* frameBuffer, int x, int y);
EOF
cat > iaq-mk2/dashboard.cpp <<'EOF'
#include "dashboard.h"

#include <string.h>

#include "air_status.h"
#include "config.h"
#include "text_format.h"
#include "src/waveshare/EPD_2in9g.h"
#include "src/waveshare/GUI_Paint.h"
#include "src/waveshare/fonts.h"

static_assert(kFrameBufferSize == ((EPD_2IN9G_WIDTH + 3) / 4) * EPD_2IN9G_HEIGHT, "framebuffer size");

namespace {

constexpr uint8_t kBlack = EPD_2IN9G_BLACK;
constexpr uint8_t kWhite = EPD_2IN9G_WHITE;
constexpr uint8_t kYellow = EPD_2IN9G_YELLOW;
constexpr uint8_t kRed = EPD_2IN9G_RED;

// ---- Layout (pixels) --------------------------------------------------------
constexpr int kMargin = 6;
constexpr int kHeaderTextY = 1;
constexpr int kRule1Y = 19;
constexpr int kLabelY = 23;
constexpr int kValueY = 37;          // Font24 baseline row
constexpr int kDividerX = 147;
constexpr int kRule2Y = 66;
constexpr int kRowAY = 70;
constexpr int kRowBY = 84;
constexpr int kRule3Y = 98;
constexpr int kBottomY = 104;
constexpr int kRightColumnX = 154;

// ---- Primitive drawing (clipped; never relies on vendor bounds checks) -------
void setPixel(int x, int y, uint8_t colour) {
  if (x < 0 || y < 0 || x >= kDashboardWidth || y >= kDashboardHeight) return;
  Paint_SetPixel((UWORD)x, (UWORD)y, colour);
}

void fillRect(int x0, int y0, int x1, int y1, uint8_t colour) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) setPixel(x, y, colour);
}

void hLine(int x0, int x1, int y, uint8_t colour = kBlack) { fillRect(x0, y, x1, y, colour); }
void vLine(int x, int y0, int y1, uint8_t colour = kBlack) { fillRect(x, y0, x, y1, colour); }

void outlineRect(int x0, int y0, int x1, int y1, uint8_t colour) {
  hLine(x0, x1, y0, colour);
  hLine(x0, x1, y1, colour);
  vLine(x0, y0, y1, colour);
  vLine(x1, y0, y1, colour);
}

int textWidth(const char* s, const sFONT& font) { return (int)strlen(s) * (int)font.Width; }

// Draws text left-aligned at (x, y) without wrapping. Glyphs that would cross
// the right edge are dropped. Non-ASCII characters render as '?'.
void drawText(int x, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite) {
  if (y < 0 || y + (int)font.Height > kDashboardHeight) return;
  for (; *s != '\0'; ++s, x += (int)font.Width) {
    if (x < 0) continue;
    if (x + (int)font.Width > kDashboardWidth) return;
    char c = *s;
    if (c < ' ' || c > '~') c = '?';
    // Paint_DrawChar has the documented (foreground, background) order.
    Paint_DrawChar((UWORD)x, (UWORD)y, c, const_cast<sFONT*>(&font), fg, bg);
  }
}

void drawTextRight(int xRight, int y, const char* s, const sFONT& font, uint8_t fg, uint8_t bg = kWhite) {
  drawText(xRight - textWidth(s, font), y, s, font, fg, bg);
}

// Small degree sign: a 3x3 ring with its top-left corner at (x, y).
void drawDegree(int x, int y, uint8_t colour) {
  setPixel(x + 1, y, colour);
  setPixel(x, y + 1, colour);
  setPixel(x + 2, y + 1, colour);
  setPixel(x + 1, y + 2, colour);
}

uint8_t levelColour(Level level) {
  switch (level) {
    case Level::Bad: return kRed;
    case Level::Warn: return kYellow;
    default: return kBlack;
  }
}

// Short hint shown next to "--" when a value is not valid.
const char* stateHint(SensorState s) {
  switch (s) {
    case SensorState::NotReady: return "init";
    case SensorState::WarmingUp: return "warm-up";
    case SensorState::Stale: return "no data";
    case SensorState::Unavailable: return "n/a";
    case SensorState::Ok: return "";
  }
  return "";
}

// ---- Sections ----------------------------------------------------------------
void drawHeader(const Readings& r, uint32_t uptimeSeconds) {
  drawText(kMargin - 2, kHeaderTextY, "AIR QUALITY", Font16, kBlack);

  const Level overall = overallLevel(r);
  const char* status = levelName(overall);
  const int badgeTextW = textWidth(status, Font16);
  const int badgeRight = kDashboardWidth - 4;
  const int badgeLeft = badgeRight - badgeTextW - 9;
  const int badgeTop = 0;
  const int badgeBottom = 17;

  uint8_t fill = kWhite;
  uint8_t ink = kBlack;
  if (overall == Level::Bad) { fill = kRed; ink = kWhite; }
  if (overall == Level::Warn) { fill = kYellow; ink = kBlack; }
  if (fill != kWhite) {
    fillRect(badgeLeft, badgeTop, badgeRight, badgeBottom, fill);
  } else if (overall == Level::Good) {
    outlineRect(badgeLeft, badgeTop, badgeRight, badgeBottom, kBlack);
  }
  drawText(badgeLeft + 5, kHeaderTextY, status, Font16, ink, fill);

  char up[20];
  char text[28] = "up ";
  formatUptime(up, sizeof up, uptimeSeconds);
  strncat(text, up, sizeof text - strlen(text) - 1);
  drawTextRight(badgeLeft - 8, kHeaderTextY + 3, text, Font12, kBlack);
}

// Big value tile: label, Font24 value in the level colour, Font12 unit.
void drawTile(int x, const char* label, const char* value, const char* unit, uint8_t valueColour,
              bool valid, SensorState state) {
  drawText(x, kLabelY, label, Font12, kBlack);
  if (valid) {
    drawText(x, kValueY, value, Font24, valueColour);
    const int unitX = x + textWidth(value, Font24) + 5;
    drawText(unitX, kValueY + (int)Font24.Height - (int)Font12.Height, unit, Font12, kBlack);
  } else {
    drawText(x, kValueY, "--", Font24, kBlack);
    const int hintX = x + textWidth("--", Font24) + 5;
    drawText(hintX, kValueY + (int)Font24.Height - (int)Font12.Height, stateHint(state), Font12, kBlack);
  }
}

void drawMainTiles(const Readings& r) {
  char value[12];

  formatUInt(value, sizeof value, r.co2.ppm, 1);
  drawTile(kMargin, "CO2", value, "ppm", levelColour(co2Level(r.co2)), r.co2.state == SensorState::Ok, r.co2.state);

  formatUInt(value, sizeof value, r.pm.pm25, 1);
  drawTile(kRightColumnX, "PM2.5", value, "ug/m3", levelColour(pm25Level(r.pm)), r.pm.state == SensorState::Ok,
           r.pm.state);

  vLine(kDividerX, kRule1Y + 2, kRule2Y - 2);
}

void drawSecondaryRows(const Readings& r) {
  char line[40];
  char num[12];

  // Row A left: PM1 / PM10
  if (r.pm.state == SensorState::Ok) {
    char pm1[8], pm10[8];
    formatUInt(pm1, sizeof pm1, r.pm.pm1, 1);
    formatUInt(pm10, sizeof pm10, r.pm.pm10, 1);
    strcpy(line, "PM1 ");
    strcat(line, pm1);
    strcat(line, "  PM10 ");
    strcat(line, pm10);
  } else {
    strcpy(line, "PM1 --  PM10 --");
  }
  drawText(kMargin, kRowAY, line, Font12, kBlack);

  // Row A right: AQI (coloured) and TVOC
  const bool ensOk = r.ens.state == SensorState::Ok;
  drawText(kRightColumnX, kRowAY, "AQI ", Font12, kBlack);
  if (ensOk) {
    formatUInt(num, sizeof num, r.ens.aqi, 1);
    drawText(kRightColumnX + textWidth("AQI ", Font12), kRowAY, num, Font12, levelColour(aqiLevel(r.ens)));
    formatUInt(num, sizeof num, r.ens.tvocPpb, 1);
    strcpy(line, "TVOC ");
    strcat(line, num);
    strcat(line, " ppb");
  } else {
    drawText(kRightColumnX + textWidth("AQI ", Font12), kRowAY, "-", Font12, kBlack);
    strcpy(line, "TVOC --");
  }
  drawText(kRightColumnX + 42, kRowAY, line, Font12, kBlack);

  // Row B left: BME680 gas resistance
  if (r.env.state == SensorState::Ok) {
    formatFixed(num, sizeof num, r.env.gasKOhm, 0);
    strcpy(line, "GAS ");
    strcat(line, num);
    strcat(line, " kOhm");
  } else {
    strcpy(line, "GAS --");
  }
  drawText(kMargin, kRowBY, line, Font12, kBlack);

  // Row B right: ENS160 health hint when it is not delivering valid data
  if (!ensOk) {
    strcpy(line, "ENS160 ");
    strcat(line, r.ens.state == SensorState::WarmingUp && r.ens.validityFlag == 2 ? "start-up" : stateHint(r.ens.state));
    drawText(kRightColumnX, kRowBY, line, Font12, kBlack);
  }
}

void drawEnvironment(const EnvReading& env) {
  char num[12];
  if (env.state != SensorState::Ok) {
    const char* msg = env.state == SensorState::Unavailable ? "BME680 not found" : "BME680 --";
    drawText(kMargin, kBottomY, msg, Font16, kBlack);
    return;
  }
  // Temperature with a degree ring, e.g. "22.6 oC"
  formatFixed(num, sizeof num, env.temperatureC, 1);
  drawText(kMargin, kBottomY, num, Font16, kBlack);
  int x = kMargin + textWidth(num, Font16) + 3;
  drawDegree(x, kBottomY + 2, kBlack);
  drawText(x + 5, kBottomY, "C", Font16, kBlack);

  formatFixed(num, sizeof num, env.humidityPct, 0);
  char rh[12];
  strcpy(rh, num);
  strcat(rh, " %");
  drawText(118, kBottomY, rh, Font16, kBlack);

  formatFixed(num, sizeof num, env.pressureHpa, 0);
  char p[16];
  strcpy(p, num);
  strcat(p, " hPa");
  drawTextRight(kDashboardWidth - kMargin, kBottomY, p, Font16, kBlack);
}

}  // namespace

void dashboardRender(const Readings& r, uint32_t uptimeSeconds, uint8_t* frameBuffer) {
  Paint_NewImage(frameBuffer, EPD_2IN9G_WIDTH, EPD_2IN9G_HEIGHT, cfg::DISPLAY_ROTATION, kWhite);
  Paint_SetScale(4);
  Paint_SelectImage(frameBuffer);
  Paint_Clear(kWhite);

  drawHeader(r, uptimeSeconds);
  hLine(0, kDashboardWidth - 1, kRule1Y);
  drawMainTiles(r);
  hLine(0, kDashboardWidth - 1, kRule2Y);
  drawSecondaryRows(r);
  hLine(0, kDashboardWidth - 1, kRule3Y);
  drawEnvironment(r.env);
}

uint8_t dashboardPixel(const uint8_t* frameBuffer, int x, int y) {
  if (x < 0 || y < 0 || x >= kDashboardWidth || y >= kDashboardHeight) return kWhite;
  // Inverse of Paint_SetPixel for rotation 90 / 270 with WidthMemory = 128.
  int mx, my;
  if (cfg::DISPLAY_ROTATION == 90) {
    mx = EPD_2IN9G_WIDTH - y - 1;
    my = x;
  } else {
    mx = y;
    my = EPD_2IN9G_HEIGHT - x - 1;
  }
  const uint8_t byte = frameBuffer[mx / 4 + my * (EPD_2IN9G_WIDTH / 4)];
  return (uint8_t)((byte >> (6 - 2 * (mx % 4))) & 0x3);
}
EOF
```

- [ ] **Step 4: Run tests and render previews**

Run: `make -C tests test && make -C tests preview`
Expected: 26 tests, 0 failures; six PNGs in `tests/out/`. Open them (Read tool) and check: no text crosses the right edge, badge colours match status, "--" states readable, degree ring visible.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: add dashboard renderer with host previews"
```

---

### Task 8: CO2 and PM input glue (Arduino)

**Files:**
- Create: `iaq-mk2/co2_input.h`, `iaq-mk2/co2_input.cpp`, `iaq-mk2/pm_input.h`, `iaq-mk2/pm_input.cpp`

**Interfaces:**
- Consumes: `co2FromPulse`, `Pms7003Parser`, `cfg::*`, `Co2Reading`, `PmReading`.
- Produces: `namespace co2_input { void begin(); void update(uint32_t nowMs, Co2Reading& out); uint32_t cyclesSeen(); uint32_t cyclesRejected(); }`; `namespace pm_input { void begin(); void update(uint32_t nowMs, PmReading& out); uint32_t framesOk(); uint32_t checksumErrors(); }`.

- [ ] **Step 1: Implement**

```bash
cat > iaq-mk2/co2_input.h <<'EOF'
#pragma once
#include <stdint.h>
#include "readings.h"

// SEN0219 PWM capture on cfg::PIN_CO2_PWM using a CHANGE interrupt.
namespace co2_input {
void begin();
// Converts the latest complete PWM cycle and applies warm-up / stale rules.
void update(uint32_t nowMs, Co2Reading& out);
uint32_t cyclesSeen();
uint32_t cyclesRejected();
}  // namespace co2_input
EOF
cat > iaq-mk2/co2_input.cpp <<'EOF'
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
EOF
cat > iaq-mk2/pm_input.h <<'EOF'
#pragma once
#include <stdint.h>
#include "readings.h"

// PMS7003 on Serial1 (D0 RX / D1 TX), active mode, 9600 baud.
namespace pm_input {
void begin();
// Drains the UART into the parser and applies warm-up / stale rules.
void update(uint32_t nowMs, PmReading& out);
uint32_t framesOk();
uint32_t checksumErrors();
}  // namespace pm_input
EOF
cat > iaq-mk2/pm_input.cpp <<'EOF'
#include "pm_input.h"

#include <Arduino.h>

#include "config.h"
#include "pms7003.h"

namespace {
Pms7003Parser g_parser;
PmsFrame g_frame;
uint32_t g_lastFrameMs = 0;
bool g_everValid = false;
}  // namespace

void pm_input::begin() { Serial1.begin(cfg::PMS_BAUD); }

void pm_input::update(uint32_t nowMs, PmReading& out) {
  // Bound the work per call; the ring buffer holds 256 bytes anyway.
  for (int i = 0; i < 256 && Serial1.available() > 0; ++i) {
    if (g_parser.feed((uint8_t)Serial1.read(), g_frame)) {
      out.pm1 = g_frame.pm1Atm;
      out.pm25 = g_frame.pm25Atm;
      out.pm10 = g_frame.pm10Atm;
      g_lastFrameMs = nowMs;
      g_everValid = true;
    }
  }

  if (!g_everValid) {
    out.state = SensorState::NotReady;
    return;
  }
  if (nowMs - g_lastFrameMs > cfg::PMS_STALE_MS) {
    out.state = SensorState::Stale;
  } else if (nowMs < cfg::PMS_WARMUP_MS) {
    out.state = SensorState::WarmingUp;
  } else {
    out.state = SensorState::Ok;
  }
}

uint32_t pm_input::framesOk() { return g_parser.framesOk(); }
uint32_t pm_input::checksumErrors() { return g_parser.checksumErrors(); }
EOF
```

- [ ] **Step 2: Verify the host tests still pass (glue files are not part of the host build)**

Run: `make -C tests test`
Expected: 26 tests, 0 failures.

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: add SEN0219 interrupt capture and PMS7003 UART input"
```

---

### Task 9: BME680 + ENS160 glue (Arduino)

**Files:**
- Create: `iaq-mk2/env_sensors.h`, `iaq-mk2/env_sensors.cpp`

**Interfaces:**
- Consumes: Adafruit_BME680 (`begin(addr)`, `setTemperatureOversampling`, `setHumidityOversampling`, `setPressureOversampling`, `setIIRFilterSize`, `setGasHeater`, `beginReading()` -> end time ms or 0, `endReading()`, fields `temperature`, `humidity`, `pressure` (Pa), `gas_resistance` (ohm)); DFRobot_ENS160_I2C (`begin()` -> 0 OK, `setTempAndHum(t, rh)`, `getENS160Status()` validity 0..3, `getAQI()`, `getTVOC()`, `getECO2()`).
- Produces: `namespace env_sensors { void begin(EnvReading& env, EnsReading& ens); void update(uint32_t nowMs, EnvReading& env, EnsReading& ens); bool bmePresent(); bool ensPresent(); }`.

- [ ] **Step 1: Implement**

```bash
cat > iaq-mk2/env_sensors.h <<'EOF'
#pragma once
#include <stdint.h>
#include "readings.h"

// BME680 (temperature, humidity, pressure, gas) and ENS160 (AQI, TVOC, eCO2)
// on the I2C bus. BME680 is read asynchronously; its result feeds the ENS160
// temperature/humidity compensation before the ENS160 is read.
namespace env_sensors {
void begin(EnvReading& env, EnsReading& ens);
void update(uint32_t nowMs, EnvReading& env, EnsReading& ens);
bool bmePresent();
bool ensPresent();
}  // namespace env_sensors
EOF
cat > iaq-mk2/env_sensors.cpp <<'EOF'
#include "env_sensors.h"

#include <Adafruit_BME680.h>
#include <Arduino.h>
#include <DFRobot_ENS160.h>
#include <Wire.h>

#include "config.h"

namespace {

Adafruit_BME680 g_bme(&Wire);
DFRobot_ENS160_I2C g_ens(&Wire, cfg::ENS160_ADDRESS);

bool g_bmePresent = false;
bool g_ensPresent = false;

uint32_t g_nextTickMs = 0;
bool g_readingInProgress = false;
uint32_t g_readingEndMs = 0;
uint32_t g_lastEnvOkMs = 0;
uint8_t g_envFailures = 0;
constexpr uint8_t kMaxEnvFailures = 3;

bool initBme() {
  for (const uint8_t addr : cfg::BME680_ADDRESSES) {
    if (!g_bme.begin(addr)) continue;
    g_bme.setTemperatureOversampling(BME680_OS_8X);
    g_bme.setHumidityOversampling(BME680_OS_2X);
    g_bme.setPressureOversampling(BME680_OS_4X);
    g_bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    g_bme.setGasHeater(320, 150);  // 320 C for 150 ms
    return true;
  }
  return false;
}

bool initEns() {
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (g_ens.begin() == NO_ERR) return true;
    delay(100);
  }
  return false;
}

void readEns(const EnvReading& env, EnsReading& out) {
  if (!g_ensPresent) return;
  if (env.state == SensorState::Ok) g_ens.setTempAndHum(env.temperatureC, env.humidityPct);

  const uint8_t validity = g_ens.getENS160Status();  // 0 normal, 1 warm-up, 2 initial start-up, 3 invalid
  out.validityFlag = validity;
  out.aqi = g_ens.getAQI();
  out.tvocPpb = g_ens.getTVOC();
  out.eco2Ppm = g_ens.getECO2();

  if (out.aqi < 1 || out.aqi > 5) {
    out.state = SensorState::Stale;  // nothing sensible on the bus
  } else if (validity == 0) {
    out.state = SensorState::Ok;
  } else if (validity == 1 || validity == 2) {
    out.state = SensorState::WarmingUp;
  } else {
    out.state = SensorState::Stale;
  }
}

void envFailed(EnvReading& env) {
  if (g_envFailures < 255) ++g_envFailures;
  if (g_envFailures >= kMaxEnvFailures && env.state == SensorState::Ok) env.state = SensorState::Stale;
}

}  // namespace

void env_sensors::begin(EnvReading& env, EnsReading& ens) {
  Wire.begin();
  g_bmePresent = initBme();
  g_ensPresent = initEns();
  env.state = g_bmePresent ? SensorState::NotReady : SensorState::Unavailable;
  ens.state = g_ensPresent ? SensorState::NotReady : SensorState::Unavailable;
  g_nextTickMs = millis();
}

void env_sensors::update(uint32_t nowMs, EnvReading& env, EnsReading& ens) {
  // Start a sampling tick.
  if (!g_readingInProgress && (int32_t)(nowMs - g_nextTickMs) >= 0) {
    g_nextTickMs = nowMs + cfg::ENV_INTERVAL_MS;
    if (g_bmePresent) {
      const uint32_t endMs = g_bme.beginReading();
      if (endMs == 0) {
        envFailed(env);
        readEns(env, ens);
      } else {
        g_readingInProgress = true;
        g_readingEndMs = endMs;
      }
    } else {
      readEns(env, ens);
    }
  }

  // Finish an in-flight BME680 reading.
  if (g_readingInProgress && (int32_t)(nowMs - g_readingEndMs) >= 0) {
    g_readingInProgress = false;
    if (g_bme.endReading()) {
      env.temperatureC = g_bme.temperature;
      env.humidityPct = g_bme.humidity;
      env.pressureHpa = (float)g_bme.pressure / 100.0f;
      env.gasKOhm = (float)g_bme.gas_resistance / 1000.0f;
      env.state = SensorState::Ok;
      g_lastEnvOkMs = nowMs;
      g_envFailures = 0;
    } else {
      envFailed(env);
    }
    readEns(env, ens);
  }

  if (env.state == SensorState::Ok && nowMs - g_lastEnvOkMs > cfg::ENV_STALE_MS) env.state = SensorState::Stale;
}

bool env_sensors::bmePresent() { return g_bmePresent; }
bool env_sensors::ensPresent() { return g_ensPresent; }
EOF
```

- [ ] **Step 2: Commit**

```bash
git add -A && git commit -m "feat: add BME680 async reading and ENS160 compensation"
```

---

### Task 10: E-paper refresh state machine (Arduino)

**Files:**
- Create: `iaq-mk2/epaper.h`, `iaq-mk2/epaper.cpp`

**Interfaces:**
- Consumes: `DEV_Module_Init`, `EPD_2IN9G_Init`, `EPD_2IN9G_Init_Fast`, `EPD_2IN9G_Display_Start`, `EPD_2IN9G_IsBusy`, `EPD_2IN9G_Sleep`, `cfg::EPD_REFRESH_TIMEOUT_MS`.
- Produces: `class EPaper { void begin(); bool requestRefresh(const uint8_t* fb, bool fullRefresh); bool update(uint32_t nowMs); bool busy() const; uint32_t refreshCount() const; uint32_t lastDurationMs() const; bool lastTimedOut() const; }` where `update` returns true once when a refresh completes.

- [ ] **Step 1: Implement**

```bash
cat > iaq-mk2/epaper.h <<'EOF'
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
  uint32_t refreshCount() const { return refreshCount_; }
  uint32_t lastDurationMs() const { return lastDurationMs_; }
  bool lastTimedOut() const { return lastTimedOut_; }
  bool lastWasFull() const { return lastFull_; }

 private:
  enum class State : uint8_t { Idle, Refreshing };
  static constexpr uint32_t kBusySettleMs = 200;  // BUSY asserts shortly after the refresh command

  State state_ = State::Idle;
  uint32_t startedMs_ = 0;
  uint32_t refreshCount_ = 0;
  uint32_t lastDurationMs_ = 0;
  bool lastTimedOut_ = false;
  bool lastFull_ = true;
};
EOF
cat > iaq-mk2/epaper.cpp <<'EOF'
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
EOF
```

- [ ] **Step 2: Commit**

```bash
git add -A && git commit -m "feat: add non-blocking e-paper refresh state machine"
```

---

### Task 11: Serial log, main sketch, firmware compile

**Files:**
- Create: `iaq-mk2/serial_log.h`, `iaq-mk2/serial_log.cpp`, `iaq-mk2/iaq-mk2.ino`

**Interfaces:**
- Consumes: everything above.
- Produces: `namespace serial_log { void begin(); void event(const char* tag, const char* message); void eventValue(const char* tag, const char* message, uint32_t value); void summary(const Readings& r, uint32_t uptimeSeconds); }`.

- [ ] **Step 1: Implement**

```bash
cat > iaq-mk2/serial_log.h <<'EOF'
#pragma once
#include <stdint.h>
#include "readings.h"

// USB serial logging. One key=value summary line per LOG_INTERVAL_MS plus
// tagged event lines. Never blocks when no terminal is attached.
namespace serial_log {
void begin();
void event(const char* tag, const char* message);
void eventValue(const char* tag, const char* message, uint32_t value);
void summary(const Readings& r, uint32_t uptimeSeconds);
}  // namespace serial_log
EOF
cat > iaq-mk2/serial_log.cpp <<'EOF'
#include "serial_log.h"

#include <Arduino.h>

#include "air_status.h"
#include "co2_input.h"
#include "config.h"
#include "pm_input.h"
#include "text_format.h"

namespace {
void kv(const char* key, const char* value) {
  Serial.print(' ');
  Serial.print(key);
  Serial.print('=');
  Serial.print(value);
}
void kvUInt(const char* key, uint32_t value) {
  Serial.print(' ');
  Serial.print(key);
  Serial.print('=');
  Serial.print(value);
}
void kvFixed(const char* key, float value, uint8_t decimals) {
  char b[16];
  formatFixed(b, sizeof b, value, decimals);
  kv(key, b);
}
}  // namespace

void serial_log::begin() { Serial.begin(cfg::SERIAL_BAUD); }

void serial_log::event(const char* tag, const char* message) {
  Serial.print('[');
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

void serial_log::eventValue(const char* tag, const char* message, uint32_t value) {
  Serial.print('[');
  Serial.print(tag);
  Serial.print("] ");
  Serial.print(message);
  Serial.println(value);
}

void serial_log::summary(const Readings& r, uint32_t uptimeSeconds) {
  Serial.print("t=");
  Serial.print(uptimeSeconds);
  kv("status", levelName(overallLevel(r)));

  kvUInt("co2", r.co2.ppm);
  kv("co2_state", sensorStateName(r.co2.state));
  kvUInt("co2_cycles", co2_input::cyclesSeen());
  kvUInt("co2_rejected", co2_input::cyclesRejected());

  kvUInt("pm1", r.pm.pm1);
  kvUInt("pm25", r.pm.pm25);
  kvUInt("pm10", r.pm.pm10);
  kv("pm_state", sensorStateName(r.pm.state));
  kvUInt("pm_frames", pm_input::framesOk());
  kvUInt("pm_crc_err", pm_input::checksumErrors());

  kvUInt("aqi", r.ens.aqi);
  kvUInt("tvoc", r.ens.tvocPpb);
  kvUInt("eco2", r.ens.eco2Ppm);
  kvUInt("ens_validity", r.ens.validityFlag);
  kv("ens_state", sensorStateName(r.ens.state));

  kvFixed("temp", r.env.temperatureC, 1);
  kvFixed("rh", r.env.humidityPct, 1);
  kvFixed("press", r.env.pressureHpa, 1);
  kvFixed("gas", r.env.gasKOhm, 1);
  kv("env_state", sensorStateName(r.env.state));
  Serial.println();
}
EOF
cat > iaq-mk2/iaq-mk2.ino <<'EOF'
/*
 * IAQ-MK2: indoor air-quality station.
 * Arduino Nano RP2040 Connect + BME680 + ENS160 + PMS7003 + SEN0219 +
 * Waveshare 2.9" e-Paper (G). See README.md for wiring and build steps.
 *
 * loop() is cooperative: inputs update `readings`, the dashboard and the
 * serial log are pure functions of it, and the e-paper refresh is polled.
 */
#include <Arduino.h>

#include "co2_input.h"
#include "config.h"
#include "dashboard.h"
#include "env_sensors.h"
#include "epaper.h"
#include "pm_input.h"
#include "readings.h"
#include "serial_log.h"
#include "watchdog.h"

namespace {
Readings readings;
EPaper epaper;
uint8_t frameBuffer[kFrameBufferSize];

uint32_t lastLogMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t displayRequests = 0;

uint32_t uptimeSeconds(uint32_t nowMs) { return nowMs / 1000; }

void pollInputs(uint32_t nowMs) {
  pm_input::update(nowMs, readings.pm);
  co2_input::update(nowMs, readings.co2);
  env_sensors::update(nowMs, readings.env, readings.ens);
}

void refreshDisplay(uint32_t nowMs) {
  const bool full = cfg::DISPLAY_FULL_REFRESH_EVERY <= 1 ||
                    (displayRequests % cfg::DISPLAY_FULL_REFRESH_EVERY) == 0;
  dashboardRender(readings, uptimeSeconds(nowMs), frameBuffer);
  if (epaper.requestRefresh(frameBuffer, full)) {
    ++displayRequests;
    serial_log::event("epd", full ? "refresh started (full)" : "refresh started (fast)");
  } else {
    serial_log::event("warn", "display busy, refresh skipped");
  }
}
}  // namespace

void setup() {
  serial_log::begin();
  watchdog::begin(cfg::WATCHDOG_TIMEOUT_MS);
  delay(500);  // give a connected terminal a moment; never wait for one

  serial_log::event("boot", "IAQ-MK2 air quality station");
  serial_log::event("boot", watchdog::active() ? "watchdog armed" : "watchdog unavailable");

  env_sensors::begin(readings.env, readings.ens);
  serial_log::event("boot", env_sensors::bmePresent() ? "BME680 found" : "BME680 NOT found");
  serial_log::event("boot", env_sensors::ensPresent() ? "ENS160 found" : "ENS160 NOT found");

  pm_input::begin();
  co2_input::begin();
  epaper.begin();
  serial_log::event("boot", "PMS7003 UART, SEN0219 PWM capture and e-paper initialised");

  // Let the fast sensors deliver first values before the boot screen.
  const uint32_t settleStart = millis();
  while (millis() - settleStart < cfg::BOOT_SETTLE_MS) {
    watchdog::kick();
    pollInputs(millis());
    delay(10);
  }

  const uint32_t now = millis();
  serial_log::summary(readings, uptimeSeconds(now));
  refreshDisplay(now);
  lastDisplayMs = now;
  lastLogMs = now;
}

void loop() {
  const uint32_t now = millis();
  watchdog::kick();

  pollInputs(now);

  if (epaper.update(now)) {
    serial_log::eventValue("epd", epaper.lastTimedOut() ? "refresh TIMED OUT after ms=" : "refresh done, ms=",
                           epaper.lastDurationMs());
  }

  if (now - lastLogMs >= cfg::LOG_INTERVAL_MS) {
    lastLogMs = now;
    serial_log::summary(readings, uptimeSeconds(now));
  }

  if (now - lastDisplayMs >= cfg::DISPLAY_INTERVAL_MS && !epaper.busy()) {
    lastDisplayMs = now;
    refreshDisplay(now);
  }
}
EOF
```

- [ ] **Step 2: Compile the firmware**

Run (libraries directory holds the four pinned libraries):
`arduino-cli compile --fqbn arduino:mbed_nano:nanorp2040connect --warnings all --libraries <libs-dir> iaq-mk2`
Expected: `Sketch uses ... bytes`, no errors. Fix any compile error in the glue files; record flash/RAM usage.

- [ ] **Step 3: Run host tests**

Run: `make -C tests test`
Expected: 26 tests, 0 failures.

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat: add serial log and main sketch"
```

---

### Task 12: README and bring-up checklist

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write the README** covering: hardware list, wiring table (from the spec section 4), the three must-check items (SJ1 jumper, PWR pin, SEN0219 PWM level), build with arduino-cli profile and with the IDE (library list), host tests and previews, serial log format, configuration knobs, bring-up checklist (expected serial lines, expected first screen, what each `--` hint means), known limitations and next steps (WiFi/NTP clock, averaging, enclosure).

- [ ] **Step 2: Commit**

```bash
git add -A && git commit -m "docs: add README with wiring, build and bring-up checklist"
```
