#pragma once
#include <stddef.h>
#include <stdint.h>

// Small, printf-free number formatting shared by the dashboard and the serial log.
// All functions NUL-terminate, truncate to `size - 1` characters and return the
// number of characters written (0 when size == 0).

// Unsigned integer, left-padded with zeros to at least `minDigits` digits.
size_t formatUInt(char* buf, size_t size, uint32_t value, uint8_t minDigits);

// `value` with `decimals` (0..3) fractional digits, rounded half away from zero.
// NaN and infinity format as "--". A value that rounds to zero has no minus sign.
size_t formatFixed(char* buf, size_t size, float value, uint8_t decimals);

// Seconds as "12m", "3h12m" or "2d03h".
size_t formatUptime(char* buf, size_t size, uint32_t seconds);
