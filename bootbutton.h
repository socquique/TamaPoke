#pragma once
#include <stdint.h>

// Pure, wrap-safe input state machine; GPIO sampling is left to the sketch.
class BootButton {
 public:
  enum Action { NONE, SINGLE, DOUBLE, LONG };
  static constexpr uint32_t DEBOUNCE_MS = 25;
  static constexpr uint32_t DOUBLE_MS = 350;
  static constexpr uint32_t LONG_MS = 1500;

  constexpr void begin(bool pressed, uint32_t now) {
    raw_ = stable_ = pressed;
    changed_ = down_ = now;
    armed_ = !pressed;  // ignore a button held during reset until released
    pending_ = second_ = longFired_ = false;
  }
  constexpr bool active() const { return raw_ || stable_ || pending_; }

  constexpr Action update(bool pressed, uint32_t now) {
    Action action = NONE;
    if (pressed != raw_) { raw_ = pressed; changed_ = now; }
    if (stable_ != raw_ && now - changed_ >= DEBOUNCE_MS) {
      stable_ = raw_;
      if (!armed_) {
        if (!stable_) armed_ = true;
        return NONE;
      }
      if (stable_) {
        down_ = changed_;  // duration starts at the raw edge, after debounce verifies it
        longFired_ = false;
        second_ = pending_ && changed_ - released_ <= DOUBLE_MS;
        if (pending_ && !second_) { pending_ = false; action = SINGLE; }
      } else if (longFired_) {
        longFired_ = second_ = pending_ = false;
      } else if (changed_ - down_ >= LONG_MS) {
        pending_ = second_ = false;
        action = LONG;
      } else if (second_) {
        pending_ = second_ = false;
        action = DOUBLE;
      } else {
        released_ = changed_;
        pending_ = true;
      }
    }
    if (!armed_) return NONE;
    // A second held press suppresses the pending single until it resolves.
    if (stable_ && raw_ && !longFired_ && now - down_ >= LONG_MS) {
      longFired_ = true;
      pending_ = second_ = false;
      return LONG;
    }
    // Do not expire during a possible second press's debounce interval.
    if (pending_ && !stable_ && !raw_ && now - released_ >= DOUBLE_MS) {
      pending_ = false;
      return SINGLE;
    }
    return action;
  }
 private:
  bool raw_ = false, stable_ = false, armed_ = true;
  bool pending_ = false, second_ = false, longFired_ = false;
  uint32_t changed_ = 0, down_ = 0, released_ = 0;
};
