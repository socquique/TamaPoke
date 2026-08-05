#include "audio.h"

void audioBegin() {}
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool) {}
bool audioEnabled() { return true; }
void audioSetMode(uint8_t) {}
uint8_t audioMode() { return SOUND_FULL; }
bool audioBusy() { return false; }
