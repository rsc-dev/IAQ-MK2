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
