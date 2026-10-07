#pragma once
#include <stddef.h>
#include <stdint.h>

// Unix epoch (UTC) to Europe/Warsaw wall-clock time. CET is UTC+1; CEST (UTC+2)
// runs from the last Sunday of March 01:00 UTC to the last Sunday of October
// 01:00 UTC (EU rule). Pure; no Arduino dependency.
struct LocalTime {
  int year;
  int month;   // 1..12
  int day;     // 1..31
  int hour;    // 0..23
  int minute;  // 0..59
  int second;  // 0..59
  bool dst;
};

// False when `epoch` is 0 (no time known yet).
bool localTimeWarsaw(uint32_t epoch, LocalTime& out);

// "HH:MM", or "--:--" when no time is known. Returns characters written.
size_t formatClockWarsaw(char* buf, size_t size, uint32_t epoch);
