#pragma once
#include <Arduino.h>

// Wi-Fi NTP time sync. Credentials live in NVS (namespace "nettime"), never
// in firmware. Safe to call with no creds or with the radio down: it must not
// block boot for more than the hard timeouts, and must not crash on failure.

bool netTimeHasCreds();
void netTimeSetSsid(const String &ssid);
void netTimeSetPass(const String &pass);
void netTimeClear();
String netTimeSsid();  // empty if unset; never returns the password

// Connect, SNTP, set the software (and PCF, if present) clock via rtcSetEpoch.
// Turns the radio off afterwards. Returns true only if a plausible epoch was
// obtained. Logs the result on Serial.
bool netTimeSync();

// Queue the sketch's manual Wi-Fi -> BLE flow (returns to UI before starting).
void requestTimeResync();

// Serial provisioning: WIFISSID, WIFIPASS, WIFICLR, WIFISYNC, WIFI?
// Returns true if the line was a Wi-Fi command (whether OK or ERR).
bool netSerialCommand(const String &line);
