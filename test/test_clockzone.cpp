#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../clockzone.h"
#include "../pet.h"
#include <cstring>

// Every test leaves the zone unset again (the default the rest of the suite runs in).
static void unsetZone() { mockNvsReset(); clockZoneBegin(); }

TEST(ClockZone, NoSettingIsUtcPassThrough) {
  unsetZone();
  CHECK_EQ(std::string(clockZoneName()), std::string(""));
  CHECK_EQ(std::string(getenv("TZ")), std::string("UTC0"));
  CHECK(clockZoneSelfTest());
  // Same arithmetic the firmware used before timezones existed.
  for (uint32_t e : { clockCivilEpoch(2026, 11, 10, 12, 36, 7), clockCivilEpoch(2026, 7, 1, 23, 59, 59),
                      clockCivilEpoch(2027, 1, 1, 0, 0, 0) }) {
    struct tm t = {};
    CHECK(clockLocalTime(e, t));
    CHECK_EQ((uint32_t)t.tm_hour, (e / 3600) % 24);
    CHECK_EQ((uint32_t)t.tm_min, (e / 60) % 60);
    CHECK_EQ(clockLocalDay(e), e / 86400);
  }
}
TEST(ClockZone, SavedZoneAppliesAndSurvivesRestart) {
  unsetZone();
  CHECK(clockZoneSet("JST-9"));
  const uint32_t e = clockCivilEpoch(2026, 11, 10, 12, 36, 7);
  struct tm t = {};
  CHECK(clockLocalTime(e, t));
  CHECK_EQ(t.tm_hour, 21);
  CHECK(clockZoneSelfTest());
  CHECK_EQ((uint32_t)mktime(&t), e);
  // "Reboot": the zone comes back from NVS.
  setenv("TZ", "UTC0", 1); tzset();
  clockZoneBegin();
  CHECK_EQ(std::string(clockZoneName()), std::string("JST-9"));
  clockZoneClear();
  CHECK_EQ(std::string(clockZoneName()), std::string(""));
  CHECK(clockLocalTime(e, t));
  CHECK_EQ(t.tm_hour, 12);
  unsetZone();
}
TEST(ClockZone, RejectsBadZoneStrings) {
  unsetZone();
  CHECK(!clockZoneSet(""));
  CHECK(!clockZoneSet("JST -9"));
  CHECK(!clockZoneSet("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  CHECK_EQ(std::string(clockZoneName()), std::string(""));
}
TEST(ClockZone, CtsSubtractsPhoneOffsetExactlyOnce) {
  unsetZone();
  CHECK(clockZoneSet("JST-9"));
  uint32_t e = clockCtsUtc(clockCivilEpoch(2026, 11, 10, 21, 36, 7), 36, 0);
  CHECK_EQ(e, clockCivilEpoch(2026, 11, 10, 12, 36, 7));
  struct tm t = {};
  CHECK(clockLocalTime(e, t));
  CHECK_EQ(t.tm_hour, 21);
  unsetZone();
}
TEST(ClockZone, DailyBoundaryIsActiveZoneMidnight) {
  unsetZone();
  CHECK(clockZoneSet("CET-1CEST,M3.5.0,M10.5.0/3"));
  CHECK(clockZoneSelfTest());
  struct tm t = {};
  t.tm_year = 126; t.tm_mon = 10; t.tm_mday = 10;
  t.tm_hour = 23; t.tm_min = 59; t.tm_sec = 59; t.tm_isdst = -1;
  uint32_t before = (uint32_t)mktime(&t);
  uint32_t after = before + 1;
  CHECK_EQ(clockLocalDay(after), clockLocalDay(before) + 1);
  unsetZone();
}
TEST(ClockZone, RepeatedResyncDoesNotReplayLiveAge) {
  unsetZone();
  Pet p; p.begin(); p.chooseStarter(4);
  uint32_t e = clockCivilEpoch(2026, 11, 10, 12, 0, 0);
  p.setClock(e);
  uint32_t age = p.ageMinutes;
  mockAdvanceMillis(3 * 60000);
  p.update(millis());
  CHECK_EQ(p.ageMinutes, age + 3);
  p.syncClock(e + 180);
  CHECK_EQ(p.ageMinutes, age + 3);
  p.syncClock(e + 180);
  CHECK_EQ(p.ageMinutes, age + 3);
  // Only an additional real UTC advance applies catch-up.
  p.syncClock(e + 780);
  CHECK_EQ(p.ageMinutes, age + 13);
}
TEST(ClockZone, TimezoneChangeDoesNotChangeEpochOrAge) {
  unsetZone();
  Pet p; p.begin(); p.chooseStarter(4);
  uint32_t e = clockCivilEpoch(2026, 11, 10, 12, 0, 0);
  p.setClock(e);
  uint32_t age = p.ageMinutes;
  CHECK(clockZoneSet("JST-9"));
  p.syncClock(e);
  CHECK_EQ(p.ageMinutes, age);
  CHECK_EQ(p.lastSeenEpoch, e);
  unsetZone();
}
