#include "nettime.h"
#include "rtcbat.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "esp_sntp.h"
#include "lwip/ip_addr.h"

static const char *kNs = "nettime";
static const uint32_t kConnectMs = 8000;
static const uint32_t kNtpMs = 10000;

// The boot seed is 2026-01-01 (1767225600). A year-threshold below that is
// useless as an NTP success check because the software clock is already seeded.
// Require a time at or after compile date (minus slack), which is well above
// the seed for this firmware. Floor lives in rtcCompileFloorEpoch().

static const char *kNtpName = "time.google.com";
static const char *kNtpName2 = "pool.ntp.org";
static const IPAddress kNtpFallback(216, 239, 35, 0);  // time.google.com

static String lastResult = "none";

static Preferences prefs;

static volatile bool sNtpGot = false;
static volatile uint32_t sNtpEpoch = 0;

static void ntpSyncedCb(struct timeval *tv) {
  // Called from the SNTP/tcpip task: no Serial, no heap.
  if (tv && tv->tv_sec > 0) {
    sNtpEpoch = (uint32_t)tv->tv_sec;
    sNtpGot = true;
  }
}

static bool openPrefs(bool ro) {
  return prefs.begin(kNs, ro);
}

bool netTimeHasCreds() {
  if (!openPrefs(true)) return false;
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid.length() > 0;
}

void netTimeSetSsid(const String &ssid) {
  if (!openPrefs(false)) return;
  prefs.putString("ssid", ssid);
  prefs.end();
}

void netTimeSetPass(const String &pass) {
  if (!openPrefs(false)) return;
  prefs.putString("pass", pass);
  prefs.end();
}

void netTimeClear() {
  if (!openPrefs(false)) return;
  prefs.clear();
  prefs.end();
  lastResult = "cleared";
}

String netTimeSsid() {
  if (!openPrefs(true)) return String();
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid;
}

static void sntpStop() {
  esp_sntp_set_time_sync_notification_cb(nullptr);
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
}

static void wifiOff() {
  sntpStop();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
}

static void ntpFail(const char *why) {
  lastResult = "fail";
  Serial.printf("NTP fail (%s)\n", why);
}

static bool waitForSntp(uint32_t timeoutMs, uint32_t floor) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (sNtpGot && sNtpEpoch >= floor) return true;
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      time_t now = time(nullptr);
      if (now >= (time_t)floor) {
        sNtpEpoch = (uint32_t)now;
        sNtpGot = true;
        return true;
      }
    }
    delay(50);
  }
  return sNtpGot && sNtpEpoch >= floor;
}

static void startSntpWithIp(const IPAddress &ip) {
  sntpStop();
  sNtpGot = false;
  sNtpEpoch = 0;
  esp_sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_set_time_sync_notification_cb(ntpSyncedCb);

  ip_addr_t addr;
  memset(&addr, 0, sizeof(addr));
  ipaddr_aton(ip.toString().c_str(), &addr);

  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setserver(0, &addr);
  esp_sntp_setservername(1, (char *)kNtpName);
  esp_sntp_init();
}

bool netTimeSync() {
  if (!openPrefs(true)) {
    ntpFail("NVS");
    return false;
  }
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  if (ssid.length() == 0) {
    lastResult = "noconfig";
    Serial.println("NTP skip (no Wi-Fi creds)");
    return false;
  }

  Serial.printf("Wi-Fi connecting to %s ...\n", ssid.c_str());
  Serial.flush();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (pass.length() == 0) WiFi.begin(ssid.c_str());
  else WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < kConnectMs) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    lastResult = "connect-fail";
    Serial.printf("Wi-Fi connect fail (status=%d, %lums)\n",
                  (int)WiFi.status(), (unsigned long)(millis() - t0));
    ntpFail("no Wi-Fi");
    wifiOff();
    return false;
  }
  Serial.printf("Wi-Fi ok ip=%s (%lums)\n",
                WiFi.localIP().toString().c_str(),
                (unsigned long)(millis() - t0));
  Serial.flush();

  // SNTP always produces UTC Unix seconds. Do not overwrite the civil-time
  // TZ configured at boot; the UI/day logic uses localtime_r().
  // Do not call configTime(): it can overwrite TZ and start hostname SNTP early.

  IPAddress ntpIp;
  bool dnsOk = (WiFi.hostByName(kNtpName, ntpIp) == 1) && (uint32_t)ntpIp != 0;
  if (!dnsOk) {
    dnsOk = (WiFi.hostByName(kNtpName2, ntpIp) == 1) && (uint32_t)ntpIp != 0;
  }
  if (dnsOk) {
    Serial.printf("NTP DNS %s -> %s\n", kNtpName, ntpIp.toString().c_str());
  } else {
    ntpIp = kNtpFallback;
    Serial.printf("NTP DNS fail, falling back to %s\n", ntpIp.toString().c_str());
  }
  Serial.flush();

  const uint32_t floor = rtcCompileFloorEpoch();
  startSntpWithIp(ntpIp);

  if (!waitForSntp(kNtpMs, floor)) {
    ntpFail("no SNTP reply, UDP 123 may be blocked on this network");
    wifiOff();
    return false;
  }

  uint32_t epoch = sNtpEpoch;
  if (epoch < floor) {
    ntpFail("time below compile floor");
    wifiOff();
    return false;
  }

  rtcSetEpoch(epoch);
  sntpStop();  // freeze the clock; do not let a later SNTP tick overwrite it
  wifiOff();

  // Re-read after teardown: must still be the NTP value, not the seed.
  uint32_t now = rtcEpoch();
  if (now < floor) {
    ntpFail("clock lost after radio off");
    return false;
  }

  lastResult = "ok";
  Serial.printf("NTP ok epoch=%lu\n", (unsigned long)now);
  return true;
}

bool netSerialCommand(const String &line) {
  if (line.startsWith("WIFISSID ")) {
    String ssid = line.substring(9);
    // do not trim again: handleSerial already trimmed the line; keep internal spaces
    if (ssid.length() == 0 || ssid.length() > 32) {
      Serial.println("ERR");
      return true;
    }
    netTimeSetSsid(ssid);
    Serial.println("OK");
    return true;
  }
  if (line == "WIFISSID") {
    Serial.println("ERR");
    return true;
  }
  if (line.startsWith("WIFIPASS ")) {
    String pass = line.substring(9);
    if (pass.length() > 63) {
      Serial.println("ERR");
      return true;
    }
    netTimeSetPass(pass);
    Serial.println("OK");
    return true;
  }
  if (line == "WIFIPASS") {
    // empty password = open network
    netTimeSetPass("");
    Serial.println("OK");
    return true;
  }
  if (line == "WIFICLR") {
    netTimeClear();
    Serial.println("OK");
    return true;
  }
  if (line == "WIFISYNC") {
    requestTimeResync();
    Serial.println("OK (queued; result follows on serial and screen)");
    return true;
  }
  if (line == "WIFI?") {
    String ssid = netTimeSsid();
    Serial.printf("ssid=%s configured=%d last=%s rtc=%lu wifi=%s\n",
                  ssid.length() ? ssid.c_str() : "(none)",
                  ssid.length() > 0 ? 1 : 0,
                  lastResult.c_str(),
                  (unsigned long)rtcEpoch(),
                  WiFi.getMode() == WIFI_OFF ? "off" : "on");
    Serial.println("DONE");
    return true;
  }
  return false;
}
