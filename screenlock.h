#pragma once
#include <stdint.h>

// PWR / SYS_OUT is active high on the 1.75C. Pure, wrap-safe state machine:
// only three completed short presses count; single/double/held presses do nothing.
class PwrButton {
 public:
  static constexpr uint32_t DEBOUNCE_MS = 25;
  static constexpr uint32_t GAP_MS = 350;
  static constexpr uint32_t TAP_MS = 600;

  constexpr void begin(bool pressed, uint32_t now) {
    raw_ = stable_ = pressed;
    changed_ = down_ = released_ = now;
    armed_ = !pressed;  // ignore a key already held at initialisation
    count_ = 0;
  }
  constexpr bool active() const { return raw_ || stable_ || count_; }

  constexpr bool update(bool pressed, uint32_t now) {
    if (pressed != raw_) { raw_ = pressed; changed_ = now; }
    if (stable_ != raw_ && now - changed_ >= DEBOUNCE_MS) {
      stable_ = raw_;
      if (!armed_) {
        if (!stable_) armed_ = true;
        return false;
      }
      if (stable_) {
        if (count_ && changed_ - released_ > GAP_MS) count_ = 0;
        down_ = changed_;
      } else if (changed_ - down_ >= TAP_MS) {
        count_ = 0;
      } else {
        released_ = changed_;
        if (++count_ == 3) { count_ = 0; return true; }
      }
    }
    // A held press cancels the entire sequence, not just this tap.
    if (stable_ && raw_ && now - down_ >= TAP_MS) count_ = 0;
    // Keep a possible next edge alive through its debounce interval.
    if (!stable_ && !raw_ && count_ && now - released_ > GAP_MS) count_ = 0;
    return false;
  }
 private:
  bool raw_ = false, stable_ = false, armed_ = true;
  uint8_t count_ = 0;
  uint32_t changed_ = 0, down_ = 0, released_ = 0;
};

// Volatile lock state: every reset starts unlocked. Input is blocked immediately,
// but the panel stays on for 600 ms AFTER the first "Locked" frame is flushed.
class ScreenLock {
 public:
  static constexpr uint32_t HINT_MS = 600;
  constexpr bool locked() const { return locked_; }
  constexpr bool toggle() {
    locked_ = !locked_;
    shown_ = blank_ = false;
    return locked_;
  }
  constexpr void frameShown(uint32_t now) {
    if (locked_ && !shown_) { shown_ = true; shownAt_ = now; }
  }
  constexpr bool blank(uint32_t now) {
    // Latch once blanked: a lock lasting an entire millis() period must not
    // reopen the hint/render path for 600 ms after the next rollover.
    if (locked_ && shown_ && now - shownAt_ >= HINT_MS) blank_ = true;
    return locked_ && blank_;
  }
 private:
  bool locked_ = false, shown_ = false, blank_ = false;
  uint32_t shownAt_ = 0;
};
