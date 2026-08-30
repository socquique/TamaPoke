#include <M5Cardputer.h>
#include "audio.h"

static bool s_enabled = true;

void audioBegin() {
  M5Cardputer.Speaker.setVolume(96);
}

void audioSetEnabled(bool on) {
  s_enabled = on;
  if (!on) M5Cardputer.Speaker.stop();
}

bool audioEnabled() {
  return s_enabled;
}

void sfxPlay(uint8_t id) {
  if (!s_enabled) return;

  // M5Unified's speaker queue handles these short tones without blocking
  // the game loop. The melodies are intentionally simple for the first port.
  switch (id) {
    case SFX_TAP:    M5Cardputer.Speaker.tone(2400, 35); break;
    case SFX_EAT:    M5Cardputer.Speaker.tone(1300, 65); break;
    case SFX_PLAY:   M5Cardputer.Speaker.tone(1900, 45); break;
    case SFX_HEART:  M5Cardputer.Speaker.tone(2800, 90); break;
    case SFX_HATCH:  M5Cardputer.Speaker.tone(2100, 180); break;
    case SFX_EVOLVE: M5Cardputer.Speaker.tone(1550, 260); break;
    case SFX_MEDAL:  M5Cardputer.Speaker.tone(3200, 160); break;
    case SFX_DENY:   M5Cardputer.Speaker.tone(420, 120); break;
    case SFX_BYE:    M5Cardputer.Speaker.tone(650, 220); break;
    case SFX_LEVEL:  M5Cardputer.Speaker.tone(2600, 120); break;
    default: break;
  }
}
