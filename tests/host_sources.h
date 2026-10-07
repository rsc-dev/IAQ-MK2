#pragma once
#include <string>
#include "byte_source.h"

// ByteSource over an in-memory string, for tests.
struct StringSource : ByteSource {
  std::string data;
  size_t pos = 0;
  explicit StringSource(std::string d) : data(std::move(d)) {}
  int read() override { return pos < data.size() ? (unsigned char)data[pos++] : -1; }
};
