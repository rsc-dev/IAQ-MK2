# Vendored Waveshare e-Paper driver

Panel: Waveshare 2.9" e-Paper Module (B) V4, SSD1680 controller, 296 x 128,
black/white/red (identified on the bench; see the project README).

Sources:
- `EPD_2in9b_V4.{cpp,h}`: `RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in9b_V4.{c,h}`
  from https://github.com/waveshareteam/e-Paper (master, fetched 2026-10-07),
  renamed to `.cpp`.
- `GUI_Paint.{h,cpp}`, `fonts.h`, `font8/12/16/20/24.cpp`: unmodified, from the
  Waveshare `2.9inch_e-Paper_G.zip` demo (`Arduino_R4` folder, 2025-12-02).

Replaced: `DEV_Config.{h,cpp}` (RP2040 pins from `config.h`, hardware SPI0,
watchdog kick and BUSY trace in delays, BUSY pull-up), `Debug.h` (no-op).

Patched `EPD_2in9b_V4.{h,cpp}` (search for `IAQ-MK2`):
- `EPD_2IN9B_V4_ReadBusy()` has a timeout (`EPD_BUSY_TIMEOUT_MS`) and counts timeouts;
- `EPD_2IN9B_V4_Display_Start(black, red, fast)` sends both planes and triggers the
  update without waiting; `EPD_2IN9B_V4_IsBusy()` exposes the BUSY pin (HIGH = busy).

Known vendor quirks the application works around:
- `Paint_DrawString_EN(..., fg, bg)` calls `Paint_DrawChar(..., bg, fg)`: the
  arguments are effectively swapped. Use `Paint_DrawChar` directly.
- `Paint_DrawPoint`/`Paint_DrawLine` draw one pixel up-left of the requested
  coordinate; `Paint_SetPixel` bounds check is `>` instead of `>=`.
- The red plane uses the Waveshare image convention (bit 0 = red); the driver
  inverts it on the way to the controller.
