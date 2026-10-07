# IAQ-MK2 weather and clock design

Date: 2026-10-07. Status: approved by the user (screen W1 variant), implementing.
Builds on `2026-10-06-iaq-mk2-firmware-design.md` (section 7 state).

## Goal

Show the Gdynia Wielki Kack forecast from MET Norway (yr.no's data) on the
bottom line of the panel, and the time of the last refresh in the header, using
the Nano RP2040 Connect's NINA-W102 WiFi. Indoor measurements are untouched.

## Screen (chosen)

```
 AIR QUALITY              14:32  [GOOD]
 CO2 ppm            | PM2.5 ug/m3
 684                | 8        PM1 5
                    |          PM10 11
 AQI 1 excellent              TVOC 42 ppb
 22.6°C        46 %         1012 hPa
 Gdynia 10°C  p.cloudy  3 m/s  6h 0.0 mm
```

- Units in the tile labels; big values alone. PM1/PM10 stacked inside the
  PM2.5 tile. GAS is logged only. No forecast credit on the panel (the README
  carries the attribution). Rain in the next 6 h is red from 0.5 mm.
- Offline or no forecast: `Gdynia: no forecast`; no time: `--:--`.
- Mockups: `docs/images/layouts/layout_W1_outdoor_line_*.png`.

## Data

- `GET https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=54.468&lon=18.495`
  with `User-Agent: IAQ-MK2/1.0 github.com/rsc-dev/IAQ-MK2`, `Accept: application/json`,
  `Connection: close`. HTTP/1.1 reply carries `Content-Length` (~37 KB);
  chunked encoding is supported anyway. Fetched hourly (the reply's `Expires`
  is ~30 min; MET asks not to poll faster than that); 10 min retry after a failure.
- Kept from the reply: `timeseries[0]`: `instant.details.air_temperature`,
  `instant.details.wind_speed`, `next_1_hours.summary.symbol_code` (mapped to
  a short word), `next_6_hours.details.precipitation_amount`.
- Time: NTP epoch from the NINA (`WiFi.getTime()`), converted to
  Europe/Warsaw (CET/CEST, EU rule: last Sunday of March 01:00 UTC to last
  Sunday of October 01:00 UTC).
- TLS terminates in the NINA; the HARICA root of api.met.no is in the NINA
  firmware bundle. Firmware version is logged at boot.

## Modules

| Module | Kind | Responsibility |
| --- | --- | --- |
| `clock` | pure | epoch -> local HH:MM (DST rule), `--:--` when no time |
| `http_body` | pure | HTTP/1.1 response reader over a byte source: status, content-length/chunked, body bytes |
| `forecast` | pure | extract the first `timeseries` object from the body stream (~1 KB), parse it with ArduinoJson, map the symbol code to a word; fills `OutdoorReading` |
| `net` | Arduino | WiFi state machine: short connect timeout, polled status, exponential retry (10 s .. 10 min); `connected()`, `epoch()`, `rssi()`, `firmwareVersion()` |
| `weather` | Arduino | hourly fetch over `WiFiSSLClient` when connected; `OutdoorReading` with `Ok`/`Stale` (3 h)/`NotReady`/`Unavailable`; failure counters |
| `dashboard` | pure | W1 screen above |
| `serial_log` | Arduino | `wifi=`, `rssi=`, `time=`, `wx_*` keys |

`Readings` gains `OutdoorReading outdoor` and `TimeReading time`.
ArduinoJson 7.4.3 is vendored as a single header under `src/ArduinoJson/` so
the firmware and the host tests use the same copy. WiFiNINA 2.1.1 is pinned in
`sketch.yaml`. Credentials come from `secrets.h` (git-ignored; template
`secrets.example.h`), never logged.

## Timing and robustness

- `WiFi.begin()` is called with a 1 s connect timeout and the status is
  polled from `loop()`; no 10 s block under the 8 s watchdog.
- TLS handshake blocks 1-4 s inside the NINA call. The download loop kicks
  the watchdog per chunk and gives up after 20 s without progress.
- Fetch and e-paper refresh are both driven from `loop()`; they cannot overlap.
  Lost PMS7003 bytes during a fetch are recovered by the parser's resync.
- RAM budget: WiFiNINA + a ~1.5 KB extraction buffer + a small JsonDocument.
- Failures (no WiFi, DNS, TLS, HTTP != 200, bad JSON, timeout) are counted
  and logged; the last good forecast stays until it is 3 h old. The station
  runs exactly as before without network.

## Tests

- clock: known epochs incl. both DST transitions of 2026, epoch 0.
- http_body: content-length body, chunked body, non-200 status, truncated.
- forecast: extraction and parse from `tests/fixtures/gdynia_compact.json`
  (9.8 C, 2.9 m/s, partlycloudy_day -> `p.cloudy`, 0.0 mm), symbol words,
  truncated input rejected.
- dashboard: existing canary/plane tests plus outdoor extremes
  (-25.5 C, 35 m/s, 99.9 mm, `hvy rain`), offline state.
- Bench: `tools/serial_dump.py` shows `wifi=connected`, `time=HH:MM`,
  `wx_state=ok`, panel refresh with the outdoor line.
