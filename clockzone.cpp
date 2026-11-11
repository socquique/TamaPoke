#include "clockzone.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

// NVS: namespace "clock", key "tz". Up to 47 characters, printable ASCII.
static const char *kTzNs = "clock";
static char sZone[48] = "";

static void applyZone(const char *tz) {
  snprintf(sZone, sizeof(sZone), "%s", tz ? tz : "");
  // No zone: UTC0, which is what the ESP32's libc already does with TZ unset,
  // so time is shown exactly as before timezones existed.
  setenv("TZ", sZone[0] ? sZone : "UTC0", 1);
  tzset();
}

static const char *buildZone() {
#ifdef TAMAPOKE_TZ
  return TAMAPOKE_TZ;
#else
  return "";
#endif
}

static bool zoneStringOk(const char *tz) {
  if (!tz || !tz[0]) return false;
  size_t n = strlen(tz);
  if (n >= sizeof(sZone)) return false;
  for (size_t i = 0; i < n; i++) {
    if (tz[i] <= ' ' || tz[i] > '~') return false;
  }
  return true;
}

void clockZoneBegin() {
  char saved[sizeof(sZone)] = "";
  Preferences p;
  if (p.begin(kTzNs, true)) {
    p.getString("tz", saved, sizeof(saved));
    p.end();
  }
  applyZone(zoneStringOk(saved) ? saved : buildZone());
}

const char *clockZoneName() { return sZone; }

bool clockZoneSet(const char *tz) {
  if (!zoneStringOk(tz)) return false;
  Preferences p;
  if (!p.begin(kTzNs, false)) return false;
  p.putString("tz", tz);
  p.end();
  applyZone(tz);
  return true;
}

void clockZoneClear() {
  Preferences p;
  if (p.begin(kTzNs, false)) {
    p.remove("tz");
    p.end();
  }
  applyZone(buildZone());
}

static_assert(clockCivilEpoch(1970, 1, 1, 0, 0, 0) == 0, "Unix origin");
static_assert(clockCivilEpoch(2026, 1, 1, 0, 0, 0) == 1767225600u, "UTC civil conversion");
static_assert(clockCivilEpoch(2026, 11, 10, 21, 36, 0) -
              clockCivilEpoch(2026, 11, 10, 12, 36, 0) == 32400, "JST is nine hours");
static_assert(clockCtsUtc(clockCivilEpoch(2026, 11, 10, 21, 36, 0), 36, 0) ==
              clockCivilEpoch(2026, 11, 10, 12, 36, 0), "iPhone JST CTS offset applied once");
static_assert(clockCtsUtc(clockCivilEpoch(2026, 11, 11, 0, 5, 0), 36, 0) ==
              clockCivilEpoch(2026, 11, 10, 15, 5, 0), "CTS crosses UTC date");
static_assert(clockCtsUtc(clockCivilEpoch(2026, 7, 1, 12, 0, 0), -20, 4) ==
              clockCivilEpoch(2026, 7, 1, 16, 0, 0), "CTS handles explicit DST");
static_assert(clockOfflineMinutes(1767225600, 1767225600, 0) == 0, "no epoch shift for TZ");
static_assert(clockOfflineMinutes(1767225600, 1767225900, 300000) == 0, "no live time counted twice");
static_assert(clockOfflineMinutes(1767225600, 1767226500, 300000) == 10, "only genuine offline minutes");
static_assert(clockOfflineMinutes(1767225900, 1767225600, 1000) == 0, "backwards clock is not ageing");

bool clockZoneSelfTest() {
  // Test the active TZ rather than assuming Japan. Noon samples avoid the
  // usual DST transition folds/gaps and cover both summer and winter rules.
  const uint32_t samples[] = {
    clockCivilEpoch(2026, 11, 10, 12, 36, 7),
    clockCivilEpoch(2026, 7, 1, 12, 36, 7)
  };
  for (uint32_t e : samples) {
    struct tm local = {};
    if (!clockLocalTime(e, local)) return false;
    struct tm roundTrip = local;
    roundTrip.tm_isdst = -1;
    if (mktime(&roundTrip) != (time_t)e) return false;
    if (clockLocalDay(e) != clockCivilEpoch(local.tm_year + 1900,
        local.tm_mon + 1, local.tm_mday, 0, 0, 0) / 86400) return false;
    if (!strcmp(sZone, "JST-9") &&
        (local.tm_hour != 21 || local.tm_min != 36 || local.tm_isdst != 0)) return false;
    if ((!sZone[0] || !strcmp(sZone, "UTC0")) &&
        (local.tm_hour != 12 || local.tm_min != 36 || local.tm_isdst != 0)) return false;

    // Same conversion the manual clock editor uses, independent of board TZ.
    struct tm edited = local;
    edited.tm_hour = 12; edited.tm_min = 15; edited.tm_sec = 0; edited.tm_isdst = -1;
    time_t editEpoch = mktime(&edited);
    if (editEpoch <= 0) return false;
    struct tm check = {};
    if (!clockLocalTime((uint32_t)editEpoch, check) || check.tm_year != local.tm_year ||
        check.tm_mon != local.tm_mon || check.tm_mday != local.tm_mday ||
        check.tm_hour != 12 || check.tm_min != 15 || check.tm_sec != 0) return false;
  }
  return true;
}

#ifdef ARDUINO
bool clockZoneSerialCommand(const String &line) {
  if (line == "TZ" || line == "TZ?") {
    Serial.printf("tz=%s\n", sZone[0] ? sZone : "(none, UTC pass-through)");
    Serial.println("DONE");
    return true;
  }
  if (line == "TZCLR") {
    clockZoneClear();
    Serial.printf("tz=%s\n", sZone[0] ? sZone : "(none, UTC pass-through)");
    Serial.println("OK");
    return true;
  }
  if (line.startsWith("TZ ")) {
    String tz = line.substring(3);
    tz.trim();
    if (!clockZoneSet(tz.c_str())) {
      Serial.println("ERR (POSIX TZ, e.g. TZ JST-9 or TZ CET-1CEST,M3.5.0,M10.5.0/3)");
      return true;
    }
    Serial.printf("tz=%s selftest=%s\n", sZone, clockZoneSelfTest() ? "PASS" : "FAIL");
    Serial.println("OK");
    return true;
  }
  return false;
}
#endif
