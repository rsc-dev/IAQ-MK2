#pragma once

// Minimal pull interface for streaming parsers, so the pure modules can be
// fed from an Arduino Client on the board and from a string or file on the host.
struct ByteSource {
  virtual ~ByteSource() {}
  // Next byte (0..255), or -1 when the source has ended or timed out.
  virtual int read() = 0;
};
