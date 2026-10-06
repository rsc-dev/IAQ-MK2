#include "text_format.h"

#include <math.h>

namespace {

// Appends `s` to buf[*pos..] honouring the size limit. Returns false when truncated.
bool append(char* buf, size_t size, size_t* pos, const char* s) {
  for (; *s; ++s) {
    if (*pos + 1 >= size) {
      buf[size - 1] = '\0';
      return false;
    }
    buf[(*pos)++] = *s;
  }
  buf[*pos] = '\0';
  return true;
}

bool appendUInt(char* buf, size_t size, size_t* pos, uint32_t value, uint8_t minDigits) {
  char digits[11];
  uint8_t n = 0;
  do {
    digits[n++] = (char)('0' + value % 10);
    value /= 10;
  } while (value != 0);
  while (n < minDigits && n < sizeof digits) digits[n++] = '0';
  char out[12];
  uint8_t o = 0;
  while (n > 0) out[o++] = digits[--n];
  out[o] = '\0';
  return append(buf, size, pos, out);
}

}  // namespace

size_t formatUInt(char* buf, size_t size, uint32_t value, uint8_t minDigits) {
  if (size == 0) return 0;
  size_t pos = 0;
  buf[0] = '\0';
  appendUInt(buf, size, &pos, value, minDigits);
  return pos;
}

size_t formatFixed(char* buf, size_t size, float value, uint8_t decimals) {
  if (size == 0) return 0;
  size_t pos = 0;
  buf[0] = '\0';
  if (isnan(value) || isinf(value)) {
    append(buf, size, &pos, "--");
    return pos;
  }
  if (decimals > 3) decimals = 3;
  static const uint32_t kPow10[] = {1, 10, 100, 1000};
  const uint32_t scale = kPow10[decimals];
  const bool negative = value < 0.0f;
  const float magnitude = negative ? -value : value;
  if (magnitude >= 2.0e9f / (float)scale) {
    append(buf, size, &pos, "--");
    return pos;
  }
  const uint32_t scaled = (uint32_t)(magnitude * (float)scale + 0.5f);
  if (negative && scaled != 0) append(buf, size, &pos, "-");
  appendUInt(buf, size, &pos, scaled / scale, 1);
  if (decimals > 0) {
    append(buf, size, &pos, ".");
    appendUInt(buf, size, &pos, scaled % scale, decimals);
  }
  return pos;
}

size_t formatUptime(char* buf, size_t size, uint32_t seconds) {
  if (size == 0) return 0;
  size_t pos = 0;
  buf[0] = '\0';
  const uint32_t minutes = seconds / 60;
  const uint32_t hours = minutes / 60;
  const uint32_t days = hours / 24;
  if (days > 0) {
    appendUInt(buf, size, &pos, days, 1);
    append(buf, size, &pos, "d");
    appendUInt(buf, size, &pos, hours % 24, 2);
    append(buf, size, &pos, "h");
  } else if (hours > 0) {
    appendUInt(buf, size, &pos, hours, 1);
    append(buf, size, &pos, "h");
    appendUInt(buf, size, &pos, minutes % 60, 2);
    append(buf, size, &pos, "m");
  } else {
    appendUInt(buf, size, &pos, minutes, 1);
    append(buf, size, &pos, "m");
  }
  return pos;
}
