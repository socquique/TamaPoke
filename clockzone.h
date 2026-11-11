#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Network epochs are UTC Unix seconds. The timezone only affects how they are
// shown (scene hour, clock screen) and where the pet's day starts.
//
// With no timezone set, nothing changes: TZ is UTC0 (the ESP32 default anyway),
// so a clock set by hand on the clock screen shows exactly that time, as before.
// A timezone can be set two ways (POSIX TZ strings, e.g. "JST-9" or
// "CET-1CEST,M3.5.0,M10.5.0/3"; the sign is reversed: JST-9 is UTC+9):
//   - at run time over serial: "TZ <posix>" (saved in NVS), "TZCLR", "TZ?"
//   - at build time: -DTAMAPOKE_TZ='"JST-9"' in compiler.cpp.extra_flags
// The NVS setting wins over the build-time one.
constexpr uint32_t clockCivilEpoch(int year, int mon, int day, int hour, int min, int sec) {
  int y = year - (mon <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153u * (unsigned)(mon + (mon > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)day - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
  const int64_t s = days * 86400 + (int64_t)hour * 3600 + (int64_t)min * 60 + sec;
  return (s < 0 || s > 0xFFFFFFFFLL) ? 0 : (uint32_t)s;
}

// Apply the configured timezone (NVS, else TAMAPOKE_TZ, else UTC0 = as before).
// Call first in setup(), before any civil-time use.
void clockZoneBegin();
// Active POSIX TZ string, or "" when none is set (UTC pass-through).
const char *clockZoneName();
// Save and apply a timezone (false if the string is empty, too long or bad).
bool clockZoneSet(const char *tz);
// Forget the saved timezone and go back to the build default (or none).
void clockZoneClear();
// TZ / TZ? / TZCLR / TZ <posix>; true if the line was a timezone command.
#ifdef ARDUINO
#include <Arduino.h>
bool clockZoneSerialCommand(const String &line);
#endif

inline bool clockLocalTime(uint32_t e, struct tm &out) {
  time_t t = e;
  return e && localtime_r(&t, &out) != nullptr;
}
inline uint32_t clockLocalDay(uint32_t e) {
  struct tm t = {};
  if (!clockLocalTime(e, t)) return 0;
  return clockCivilEpoch(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, 0, 0, 0) / 86400;
}
// Given a phone's local civil fields and explicit LTI offsets in 15-minute
// units, convert to UTC independent of this board's TZ. Never use mktime here.
constexpr uint32_t clockCtsUtc(uint32_t localCivilEpoch, int tz15, int dst15) {
  const int64_t e = (int64_t)localCivilEpoch - (int64_t)(tz15 + dst15) * 900;
  return e <= 0 || e > 0xFFFFFFFFLL ? 0 : (uint32_t)e;
}
// Live time is already accounted by millis()/Pet::update. Only a real epoch
// advance beyond the running anchor is offline time (never a TZ offset).
constexpr uint32_t clockOfflineMinutes(uint32_t anchor, uint32_t now, uint32_t liveMs) {
  const uint64_t expected = (uint64_t)anchor + liveMs / 1000;
  return anchor && now > expected ? (uint32_t)((now - expected) / 60) : 0;
}

// Runtime check of libc's TZ/localtime/mktime in the actual ESP32 build.
bool clockZoneSelfTest();
