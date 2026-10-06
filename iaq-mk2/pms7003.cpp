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
