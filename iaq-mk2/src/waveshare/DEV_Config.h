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
