#pragma once
#include <stdint.h>
#include <stddef.h>

// Pure strength-training input policy. Coordinates are the existing native
// 466x466 UI coordinates (both axes mirrored by the board's touch setup).
class TrainingTouch {
 public:
  enum Action : uint8_t { NONE, HIT, EXIT };
  static constexpr int EXIT_X = 153, EXIT_Y = 384, EXIT_W = 160, EXIT_H = 48;
  static constexpr uint32_t EXIT_HOLD_MS = 1000;
  static constexpr uint32_t REPORT_GAP_MS = 100;  // a missing stream cannot complete a hold
  static constexpr uint32_t POLL_MS = 20;  // restore the hardware-tested SensorLib cadence

  static constexpr bool onPanel(int x, int y) {
    return x >= 0 && x <= 466 && y >= 0 && y <= 466 &&
           (x - 233) * (x - 233) + (y - 233) * (y - 233) <= 233 * 233;
  }
  static constexpr bool inExit(int x, int y) {
    return x >= EXIT_X && x < EXIT_X + EXIT_W && y >= EXIT_Y && y < EXIT_Y + EXIT_H;
  }
  constexpr void reset(bool requireRelease = false) {
    down_ = false; blocked_ = requireRelease; exitArmed_ = false;
    exitStart_ = lastReport_ = 0;
  }
  constexpr bool down() const { return down_; }
  constexpr bool waitingRelease() const { return blocked_; }
  constexpr uint32_t exitHeldMs(uint32_t now) const {
    return exitArmed_ && now - lastReport_ <= REPORT_GAP_MS ? now - exitStart_ : 0;
  }
  constexpr Action update(bool pressed, int x, int y, uint32_t now) {
    if (blocked_) {
      down_ = pressed;
      if (!pressed) blocked_ = false;
      return NONE;
    }
    bool start = pressed && !down_;
    if (down_ && now - lastReport_ > REPORT_GAP_MS) exitArmed_ = false;
    down_ = pressed;
    lastReport_ = now;
    if (!pressed) { exitArmed_ = false; return NONE; }
    if (start) {
      exitArmed_ = onPanel(x, y) && inExit(x, y);
      exitStart_ = now;
      return onPanel(x, y) && !exitArmed_ ? HIT : NONE;
    }
    // Sliding into the button or back in after leaving it never arms exit.
    if (!onPanel(x, y) || !inExit(x, y)) exitArmed_ = false;
    if (exitArmed_ && now - exitStart_ >= EXIT_HOLD_MS) {
      exitArmed_ = false;
      blocked_ = true;  // exit exactly once; swallow this contact on the next screen
      return EXIT;
    }
    return NONE;
  }
 private:
  bool down_ = false, blocked_ = false, exitArmed_ = false;
  uint32_t exitStart_ = 0, lastReport_ = 0;
};

struct TrainingReport {
  bool valid = false, pressed = false;
  int16_t x = 0, y = 0;
  enum Kind : uint8_t { ERROR, CONTACT, RELEASE, EMPTY, COVER };
  Kind kind = ERROR;
  bool attempted = false, irq = false;
  uint8_t bytes = 0, raw[12] = {};
};

// Match SensorLib 0.5.1 getTouchPoints()/getPoint() semantics, used by the
// pre-regression firmware: a completed empty/ACK/filtered report means zero
// points, NOT "keep the previous finger down". 0x06 means contact present,
// not a unique DOWN event; track IDs are slots, not monotonic tap counters.
// Never count every IRQ/contact frame or invent timed repeat strikes.
constexpr TrainingReport decodeTrainingReport(const uint8_t *b, size_t n) {
  TrainingReport r;
  r.attempted = true;
  r.bytes = n > 255 ? 255 : (uint8_t)n;
  for (size_t i = 0; i < n && i < sizeof(r.raw); ++i) r.raw[i] = b[i];
  if (n < 7) return r;
  r.valid = true;
  r.kind = TrainingReport::EMPTY;
  if (b[0] == 0xAB || b[6] != 0xAB) return r;
  if (b[0] == 0) { r.kind = TrainingReport::RELEASE; return r; }
  if (b[4] & 0x80) { r.kind = TrainingReport::COVER; return r; }
  uint8_t count = b[5] & 0x7F;
  if (count > 2) return r;
  if (!count) { r.kind = TrainingReport::RELEASE; return r; }
  // Like SensorLib, choose the FIRST ACTIVE point, not blindly record zero.
  // For two fingers the first slot may be up while the second is still down.
  for (uint8_t i = 0; i < count; ++i) {
    size_t offset = i ? 7 : 0;
    if (n < offset + 5) { r.valid = false; r.kind = TrainingReport::ERROR; return r; }
    const uint8_t *p = b + offset;
    if ((p[0] & 0x0F) != 0x06 || (p[0] >> 4) >= 2) continue;
    int x = (p[1] << 4) | (p[3] >> 4);
    int y = (p[2] << 4) | (p[3] & 0x0F);
    if (x > 466 || y > 466) { r.valid = false; r.kind = TrainingReport::ERROR; return r; }
    r.pressed = true; r.kind = TrainingReport::CONTACT;
    r.x = (int16_t)(466 - x); r.y = (int16_t)(466 - y);
    return r;
  }
  r.kind = TrainingReport::RELEASE;
  return r;
}

constexpr bool trainingBeforeDeadline(uint32_t deadline, uint32_t capturedAt) {
  return (int32_t)(deadline - capturedAt) > 0;
}
