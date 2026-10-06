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
