# IAQ-MK2 firmware design

Date: 2026-10-06
Status: approved for implementation (user supplied full requirements; built autonomously, review after the fact)

## 1. Goal

Standalone indoor air-quality station on an Arduino Nano RP2040 Connect that
continuously samples four sensors and shows a status snapshot on a Waveshare
2.9" four-colour e-paper panel. Priorities: reliability, clean modular code,
real NDIR CO2 (SEN0219) rather than ENS160 eCO2, colour only for status, few
e-paper refreshes, robust serial logging.

Out of scope (future): WiFi/NTP clock, data export, averaging/filtering beyond
simple staleness handling, enclosure.

## 2. Verified hardware facts

Sources: Arduino Nano RP2040 Connect datasheet (ABX00053), the installed
`arduino:mbed_nano` 4.5.0 variant files, Waveshare wiki + demo zip
(2.9inch_e-Paper_G.zip, driver dated 2025-12-02), Plantower PMS7003 manual,
DFRobot SEN0219 wiki, Winsen MH-Z14A manual.

| Item | Verified fact |
| --- | --- |
| Display | Waveshare 2.9" e-Paper Module (G): 296x128, black/white/yellow/red, SPI, fast refresh 11 s, full 16 s, panel 3.3 V. Pixel codes BLACK=0, WHITE=1, YELLOW=2, RED=3 (2 bpp, 4 px/byte, 9472-byte frame). |
| Display wiring | Module has **nine** wires: VCC, GND, DIN, CLK, CS, DC, RST, BUSY, **PWR**. Waveshare's driver drives PWR high (D6) to enable panel power. BUSY is LOW while busy. |
| Display driver | Only Arduino_R4 / ESP32 / RPi / STM32 variants ship. Arduino_R4 `DEV_Config` uses only `SPI.begin()` + `SPI.beginTransaction(SPISettings(4 MHz, MSBFIRST, MODE0))` + `SPI.transfer()`, all available in the mbed core. |
| Display precautions | Refresh interval >= 180 s, refresh at least every 24 h, enter sleep when idle (panel left powered is damaged), re-init after sleep before the next refresh. |
| Nano RP2040 pins | D11=MOSI(GPIO7), D12=MISO(GPIO4), D13=SCK(GPIO6, also built-in LED), A4=SDA(GPIO12), A5=SCL(GPIO13), D0=Serial1 RX(GPIO1), D1=Serial1 TX(GPIO0), D2=GPIO25. ADC ref 3.3 V, A0-A3 on RP2040. |
| Nano RP2040 power | **VUSB jumper SJ1 is open from the factory**: the "5V"/VUSB pin supplies nothing unless soldered. VIH min 2.31 V, GPIO are 3.3 V only (not 5 V tolerant), 8 mA per pin, 3V3 rail 800 mA. |
| Nano RP2040 I2C | A4/A5 have on-board 4.7 k pull-ups and carry the IMU (0x6A) and ATECC608A (0x60). No conflict with BME680 (0x76/0x77) or ENS160 (0x52/0x53). |
| PMS7003 | 5 V supply (fan), **3.3 V TTL** on TX/RX/SET/RESET, not 5 V tolerant. Frame: 0x42 0x4D, length 28, 32 bytes total, checksum = sum of bytes 0..29, atmospheric PM1.0/2.5/10 at bytes 10-15. |
| SEN0219 | SKU was re-issued: current unit is **PWM** NDIR, 400-5000 ppm, 5 V supply, interface level 3.3 V (5 V compatible), preheat 1 min, T90 < 120 s. PWM cycle 1004 ms +-5 %, 2 ms lead/trail: `ppm = 5000 * (TH - 2 ms) / (TH + TL - 4 ms)`. The older SEN0219 was analog 0.4-2 V; MK1 already read PWM on D2 successfully, so the PWM unit is assumed. |
| Libraries | `ScioSense_ENS160` (v1 API used in the proposal) is no longer published under that name; it survives as "ENS160 - Adafruit Fork" 3.0.1. Official `ScioSense_ENS16x` 2.0.5 has a different API. `DFRobot_ENS160` 1.0.1 (used in MK1) exposes the validity flag. `Adafruit BME680 Library` 2.0.6 provides async `beginReading()/endReading()`. |

## 3. Review findings on the proposed sketch

Blocking defects:

1. `Paint_DrawString_EN(x, y, s, font, fg, bg)` internally calls
   `Paint_DrawChar(..., bg, fg)`: the 5th argument becomes the background.
   The proposal passes `(colour, WHITE)`, which renders white glyphs on a
   coloured block. All text must go through a wrapper with the right order.
2. PWR pin missing from the wiring. Without PWR high the panel is unpowered.
3. 5 V rail: both 5 V sensors depend on SJ1 being soldered or on external 5 V.

Correctness / robustness:

4. CO2 formula: normalising to 1000 ms then `(TH-2)*5` deviates from the
   datasheet formula; no cycle-length plausibility check; `co2Valid` never
   expires when the sensor is unplugged.
5. PMS parser: `pms.valid` never expires; a `0x42` arriving in slot 1 drops a
   frame; frame-length field not checked.
6. Blocking: `EPD_2IN9G_Display()` spins on BUSY for 11-16 s. Serial1 has a
   256-byte RX ring; PMS7003 emits 32 B/s, so frames are lost on every
   refresh and all timers slip. `bme.performReading()` blocks ~200 ms.
7. Overall status reports GOOD when every input is invalid.
8. Layout: `"150 ug/m3"` in Font24 from x=155 is 153 px wide, past 296, and
   `Paint_DrawString_EN` wraps it onto the next row. `Paint_SetPixel` bounds
   check is `>` not `>=`, so drawing at x=296 writes past the buffer.
9. No watchdog, no staleness handling, no sensor-health reporting; `%f` in
   `snprintf` depends on printf float support in the core.
10. Vendor `Debug.h` logs "e-Paper busy" lines in the busy loop and pulls in
    `Wire.h`; the demo's fast mode forces a fake temperature (reg 0xE6 = 90),
    which trades colour fidelity for time.

Verified as correct in the proposal: display model/colour codes, SPI/CS/DC/RST/BUSY
pins, framebuffer size and rotation 90 for landscape, Paint/font signatures,
PMS7003 frame offsets and checksum, Serial1 on D0/D1, interrupt on D2, I2C
addresses, ENS160 compensation idea, refresh cadence.

## 4. Wiring (final)

| Device | Pin | Nano RP2040 Connect |
| --- | --- | --- |
| e-paper | VCC | 3V3 |
| e-paper | GND | GND |
| e-paper | DIN | D11 (MOSI) |
| e-paper | CLK | D13 (SCK) |
| e-paper | CS | D10 |
| e-paper | DC | D9 |
| e-paper | RST | D8 |
| e-paper | BUSY | D7 |
| e-paper | PWR | D6 (driven high; 3V3 also acceptable) |
| BME680 + ENS160 | SDA / SCL / VCC / GND | A4 / A5 / 3V3 / GND |
| PMS7003 | VCC / GND | 5 V (VUSB, needs SJ1) / GND |
| PMS7003 | TX | D0 (Serial1 RX) |
| PMS7003 | RX | D1 (Serial1 TX), optional |
| SEN0219 | VCC / GND | 5 V (VUSB, needs SJ1) / GND |
| SEN0219 | PWM | D2 |

Checks before power-up: confirm SJ1 or external 5 V; measure the SEN0219 PWM
high level (expect ~3.3 V, must be < 3.6 V); confirm the e-paper module has a
PWR pin (newer boards) and is at Rev 2.1 or later if VCC is ever fed 5 V.

## 5. Architecture

Cooperative `loop()` driven by `millis()`; no `delay()` outside bounded vendor
init sequences. Pure logic has no Arduino dependency and is unit-tested on the
host with g++.

```
iaq-mk2/
  iaq-mk2.ino        setup/loop: scheduling, serial log, watchdog kick
  config.h           pins, intervals, thresholds, warm-up and stale timeouts
  readings.h         Readings struct (values + per-sensor state)
  air_status.{h,cpp} pure: level per metric, overall status           [tested]
  pms7003.{h,cpp}    pure: incremental PMS7003 frame parser             [tested]
  sen0219.{h,cpp}    pure: PWM pulse -> ppm + plausibility              [tested]
  text_format.{h,cpp} pure: fixed-point number formatting              [tested]
  dashboard.{h,cpp}  pure: Readings -> framebuffer via GUI_Paint        [preview rendered on host]
  co2_input.{h,cpp}  Arduino: edge-capture ISR on D2, staleness
  pm_input.{h,cpp}   Arduino: Serial1 -> parser, staleness
  env_sensors.{h,cpp} Arduino: BME680 async read, ENS160 read + compensation
  epaper.{h,cpp}     Arduino: non-blocking refresh state machine, sleep
  watchdog.{h,cpp}   Arduino: mbed hardware watchdog
  src/waveshare/     vendored EPD_2in9g, GUI_Paint, fonts (+ own DEV_Config, Debug.h)
tests/               g++ unit tests + dashboard PNG preview renderer
sketch.yaml          arduino-cli profile pinning core and library versions
```

### Data flow

Inputs update the single `Readings` instance; the dashboard and the serial
log are pure functions of it.

- CO2: ISR records rise/fall timestamps; `co2_input` converts the latest
  complete cycle via `sen0219`, rejects cycles outside 1004 ms +-10 %,
  marks the value `WARMING_UP` until `CO2_WARMUP_MS` after boot and `STALE`
  when no cycle arrived for `CO2_STALE_MS`.
- PM: every loop drains Serial1 into the parser; a checksum-valid frame
  updates PM1/PM2.5/PM10 (atmospheric); `WARMING_UP` for `PMS_WARMUP_MS`
  after boot, `STALE` after `PMS_STALE_MS` without a frame.
- BME680: every `ENV_INTERVAL_MS` call `beginReading()`; when
  `remainingReadingMillis()` reaches 0 call `endReading()`. Consecutive
  failures mark it `UNAVAILABLE`.
- ENS160: after each BME680 result write T/RH compensation, then read
  status, AQI, TVOC, eCO2. Validity flag 0 = OK, 1 = warm-up (3 min),
  2 = initial start-up (1 h), 3 = invalid. eCO2 is logged only.

### Display policy

- Boot screen is drawn immediately (shows warm-up states), then a refresh
  every `DISPLAY_INTERVAL_MS` (180 s, the Waveshare minimum).
- Refresh is a state machine: INIT+SEND (bounded, ~1 s) -> WAIT_BUSY (polled
  from loop, with a 60 s timeout) -> SLEEP. The loop keeps running during the
  11-16 s panel update.
- Full refresh by default; `DISPLAY_FULL_REFRESH_EVERY` > 1 enables the
  vendor fast mode in between. Panel is put to deep sleep after every refresh
  and re-initialised before the next.
- Rotation is a config constant (90 or 270) because the upright direction
  depends on how the FPC is mounted.

### Layout (296x128, fonts: 8=5x8, 12=7x12, 16=11x16, 24=17x24)

```
 y0   AIR QUALITY          up 3h12m             [ GOOD ]   Font16 header, status badge
 y18  --------------------------------------------------
 y22  CO2                      |  PM2.5                   Font12 labels, vertical divider at x=147
 y36  684  ppm                 |  8  ug/m3                Font24 value, Font12 unit, coloured value
 y66  --------------------------------------------------
 y70  PM1 5   PM10 11          |  AQI 1   TVOC 42 ppb     Font12, AQI coloured
 y84  GAS 86 kOhm              |  ENS160 warm-up / blank  Font12
 y98  --------------------------------------------------
 y102 22.6 C        46 %        1012 hPa                  Font16
```

Not-ready states render as `--` with a short word (`warm-up`, `no data`).
Status badge: GOOD = black outline, WARN = yellow fill with black text,
BAD = red fill with white text, WARMUP = black text when no metric is valid.

### Thresholds (from the requirements)

CO2: <800 good, 800-1199 warn, >=1200 bad. PM2.5: <=15 good, 16-35 warn,
>35 bad. ENS160 AQI: 1 good, 2 warn, >=3 bad. Overall = worst of the valid
metrics; UNKNOWN when none is valid.

### Serial log (115200)

One `key=value` summary line per sensor interval, e.g.
`t=123 status=GOOD co2=684 co2_state=ok pm1=5 pm25=8 pm10=11 pm_state=ok aqi=1 tvoc=42 eco2=612 ens_state=ok temp=22.6 rh=46.1 press=1012.3 gas=86.2 env_state=ok`.
Event lines prefixed with `[boot]`, `[epd]`, `[warn]`, `[err]`. Serial output
never blocks when no terminal is attached.

### Reliability

- Hardware watchdog (~8 s) kicked from `loop()` and from the vendor busy-wait
  hook so a hang reboots the board; display busy-wait has a timeout so a
  missing panel degrades to a logged error rather than a reset loop.
- All sensor values carry a state; nothing is displayed as valid after its
  source stops.
- No dynamic allocation after setup; fixed 9472-byte framebuffer.

## 6. Build and test

- `arduino-cli compile --fqbn arduino:mbed_nano:nanorp2040connect iaq-mk2`
  with libraries Adafruit BME680 2.0.6, Adafruit Unified Sensor 1.1.15,
  Adafruit BusIO 1.17.4, DFRobot_ENS160 1.0.1 (pinned in `sketch.yaml`).
- `make -C tests` builds and runs host unit tests and writes dashboard
  previews (`tests/out/*.png`) for GOOD / WARN / BAD / warm-up scenarios.
- Hardware bring-up checklist in README (cannot be executed here).

## 7. Addendum: bench findings (2026-10-07)

The design above assumed the Waveshare 2.9" (G) four-colour panel. Bring-up on
the real hardware changed three things.

1. **Panel identity.** The module has eight wires (no PWR) and did not answer
   the G command set: BUSY only reacted to RST. A probe with the SSD1680
   sequence produced a 16.4 s busy period and a visible refresh, so the panel
   is the 2.9" (B) V4 (SSD1680, black/white/red, BUSY HIGH while busy). The
   vendored driver is now `EPD_2in9b_V4` with the same non-blocking patch; the
   dashboard renders a 2-bpp frame and splits it into the black and red planes.
   Without yellow, WARN is drawn as black with red marks (underline under the
   value, red-outlined badge); BAD stays red. Sections 2 and 4 are superseded
   accordingly (no PWR wire, no yellow).
2. **Watchdog versus upload.** The bootrom's `reset_usb_boot()` (1200-baud touch
   and double-tap reset) arms the hardware watchdog with a ~10 ms countdown and
   spins; the loop's kicks reloaded the 8 s timeout and cancelled the reboot,
   leaving the board un-flashable. The sketch now intercepts the 1200-baud line
   coding event itself, suspends kicking, waits for the host to close the port
   and calls the core's `_ontouch1200bps_()`. Reading the watchdog TIME field
   to detect a foreign countdown was tried first and blocked every kick; it is
   not used.
3. **Serial boot grace.** The USB CDC drops output until a terminal attaches,
   so the first log lines are preceded by a 2 s delay (`BOOT_SERIAL_DELAY_MS`)
   and `loop()` yields 1 ms per iteration to the RTOS.

Verified on hardware: BME680, ENS160, PMS7003 and SEN0219 all deliver data
(SEN0219 at the 400 ppm preheat floor during warm-up); full refresh completes
in ~16.4 s and the panel enters deep sleep; normal uploads work.
