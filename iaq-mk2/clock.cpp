#include "clock.h"

#include "text_format.h"

namespace {

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

void civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = (int64_t)yoe + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int)(yy + (m <= 2));
}

// Epoch seconds of 01:00 UTC on the last Sunday of a 31-day month.
uint32_t lastSunday0100Utc(int year, unsigned month) {
  int64_t days = daysFromCivil(year, month, 31);
  int weekday = (int)((days + 4) % 7);  // 1970-01-01 was a Thursday (4); 0 = Sunday
  if (weekday < 0) weekday += 7;
  days -= weekday;
  return (uint32_t)(days * 86400 + 3600);
}

}  // namespace

bool localTimeWarsaw(uint32_t epoch, LocalTime& out) {
  if (epoch == 0) return false;
  int y;
  unsigned m, d;
  civilFromDays((int64_t)(epoch / 86400), y, m, d);
  const bool dst = epoch >= lastSunday0100Utc(y, 3) && epoch < lastSunday0100Utc(y, 10);
  const uint32_t local = epoch + (dst ? 7200u : 3600u);
  civilFromDays((int64_t)(local / 86400), y, m, d);
  const uint32_t secondsOfDay = local % 86400;
  out.year = y;
  out.month = (int)m;
  out.day = (int)d;
  out.hour = (int)(secondsOfDay / 3600);
  out.minute = (int)(secondsOfDay % 3600 / 60);
  out.second = (int)(secondsOfDay % 60);
  out.dst = dst;
  return true;
}

size_t formatClockWarsaw(char* buf, size_t size, uint32_t epoch) {
  if (size == 0) return 0;
  LocalTime t;
  if (!localTimeWarsaw(epoch, t)) {
    const char* placeholder = "--:--";
    size_t n = 0;
    for (; placeholder[n] != '\0' && n + 1 < size; ++n) buf[n] = placeholder[n];
    buf[n] = '\0';
    return n;
  }
  char h[4], m[4];
  formatUInt(h, sizeof h, (uint32_t)t.hour, 2);
  formatUInt(m, sizeof m, (uint32_t)t.minute, 2);
  size_t n = 0;
  for (const char* p = h; *p && n + 1 < size; ++p) buf[n++] = *p;
  if (n + 1 < size) buf[n++] = ':';
  for (const char* p = m; *p && n + 1 < size; ++p) buf[n++] = *p;
  buf[n] = '\0';
  return n;
}
