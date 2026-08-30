#pragma once
#include <Arduino.h>

// Minimal language shim used by upstream dex.h.
// The Cardputer port currently renders its own English UI but dexName()
// still supports the upstream FR/DE name tables if gLang is changed.
enum Lang : uint8_t {
  LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT, LANG_COUNT
};
#define LANG_DEFAULT LANG_EN
extern Lang gLang;
