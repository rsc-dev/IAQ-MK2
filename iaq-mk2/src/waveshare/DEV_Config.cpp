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
