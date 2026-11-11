#include "trainingtouch.h"
#include <initializer_list>

// Every firmware build compiles these tests, even without a host C++ compiler.
namespace {
using T = TrainingTouch;
constexpr bool rapid() {
  T t;
  for (uint32_t n = 0; n < 100; ++n) {
    if (t.update(true, 233, 120, n * 30) != T::HIT) return false;
    if (t.update(true, 235, 122, n * 30 + 5) != T::NONE) return false;
    if (t.update(false, 0, 0, n * 30 + 15) != T::NONE) return false;
  }
  return true;
}
constexpr bool oldQuitStrip() {
  T t;
  return t.update(true, 233, 30, 0) == T::HIT &&
         t.update(true, 233, 30, 2000) == T::NONE;
}
constexpr bool broadTarget() {
  for (auto y : {80, 160, 240, 320, 370}) {
    for (auto x : {130, 233, 336}) {
      T t;
      if (t.update(true, x, y, 0) != T::HIT) return false;
    }
  }
  return true;
}
constexpr bool fastExitTaps() {
  T t;
  for (uint32_t n = 0; n < 100; ++n) {
    if (t.update(true, 233, 405, n * 50) != T::NONE) return false;
    if (t.update(false, 0, 0, n * 50 + 20) != T::NONE) return false;
  }
  return true;
}
constexpr bool hold(uint32_t start = 100) {
  T t;
  for (uint32_t elapsed = 0; elapsed < 1000; elapsed += 25)
    if (t.update(true, 233, 405, start + elapsed) != T::NONE) return false;
  if (t.update(true, 233, 405, start + 999) != T::NONE) return false;
  return t.update(true, 233, 405, start + 1000) == T::EXIT &&
         t.update(true, 233, 405, start + 2000) == T::NONE &&
         t.update(false, 0, 0, start + 2100) == T::NONE &&
         t.update(true, 233, 233, start + 2150) == T::HIT;
}
constexpr bool shortHold() {
  T t;
  for (uint32_t n = 0; n <= 975; n += 25)
    if (t.update(true, 233, 405, n) != T::NONE) return false;
  return t.update(false, 0, 0, 999) == T::NONE && t.exitHeldMs(1000) == 0;
}
constexpr bool slideOutAndBack() {
  T t; t.update(true, 233, 405, 0);
  t.update(true, 233, 370, 25);
  for (uint32_t n = 50; n <= 1500; n += 25)
    if (t.update(true, 233, 405, n) != T::NONE) return false;
  return t.exitHeldMs(1500) == 0;
}
constexpr bool slideIntoExit() {
  T t;
  if (t.update(true, 233, 233, 0) != T::HIT) return false;
  for (uint32_t n = 25; n <= 1500; n += 25)
    if (t.update(true, 233, 405, n) != T::NONE) return false;
  return true;
}
constexpr bool missingReports() {
  T t; t.update(true, 233, 405, 0);
  return t.update(true, 233, 405, 1000) == T::NONE && t.exitHeldMs(1000) == 0;
}
constexpr bool heldThroughReset() {
  T t; t.update(true, 233, 405, 0); t.reset(true);
  for (uint32_t n = 25; n < 2000; n += 25)
    if (t.update(true, 233, 405, n) != T::NONE) return false;
  return t.update(false, 0, 0, 2000) == T::NONE &&
         t.update(true, 233, 233, 2050) == T::HIT;
}
constexpr uint8_t down[] = {0x06, 0x0E, 0x0E, 0x99, 0, 1, 0xAB};
constexpr uint8_t top[] = {0x06, 0x0E, 0x1B, 0x94, 0, 1, 0xAB};  // raw (233,436) -> UI (233,30)
constexpr uint8_t up[] = {0, 0, 0, 0, 0, 0, 0xAB};
constexpr uint8_t stale[] = {0xAB, 0, 0, 0, 0, 0, 0xAB};
constexpr uint8_t bad[] = {0x06, 0x0E, 0x0E, 0x99, 0, 1, 0};
constexpr uint8_t cover[] = {0x06, 0x0E, 0x0E, 0x99, 0x80, 1, 0xAB};
constexpr uint8_t outside[] = {0x06, 0xFF, 0x0E, 0x99, 0, 1, 0xAB};
static_assert(rapid(), "100 fast taps count once each; no repeat while held");
static_assert(oldQuitStrip(), "top strip hits, never quits");
static_assert(broadTarget(), "large tap zone around and below the visible sack");
static_assert(fastExitTaps(), "repeated quick taps on exit never quit or score");
static_assert(hold(), "exit at one second exactly, once, then swallow release");
static_assert(hold(0xFFFFFF00u), "exit hold survives millis rollover");
static_assert(shortHold(), "release just before one second cancels exit");
static_assert(slideOutAndBack(), "leaving exit cancels the entire hold");
static_assert(slideIntoExit(), "a strike dragged onto exit never quits");
static_assert(missingReports(), "missing reports cannot complete an exit hold");
static_assert(heldThroughReset(), "lock/unlock discards partial exit; require release");
static_assert(!T::onPanel(0, 0) && !T::onPanel(467, 233) && T::onPanel(233, 0), "round-panel bounds");
static_assert(T::inExit(153, 384) && T::inExit(312, 431) && !T::inExit(313, 432), "exit bounds match UI");
static_assert(decodeTrainingReport(down, sizeof(down)).pressed && decodeTrainingReport(down, sizeof(down)).x == 233, "native mirrored touch coordinates");
static_assert(decodeTrainingReport(top, sizeof(top)).y == 30, "top strip mapping, no old-board scaling");
static_assert(decodeTrainingReport(up, sizeof(up)).valid && !decodeTrainingReport(up, sizeof(up)).pressed, "real release packet");
static_assert(decodeTrainingReport(stale, sizeof(stale)).valid && !decodeTrainingReport(stale, sizeof(stale)).pressed,
              "regression: ACK/empty must rearm like legacy getPoint(), not latch down");
static_assert(decodeTrainingReport(bad, sizeof(bad)).valid && !decodeTrainingReport(bad, sizeof(bad)).pressed &&
              !decodeTrainingReport(down, 6).valid, "completed filtered report is empty; short read is error");
static_assert(decodeTrainingReport(cover, sizeof(cover)).valid && !decodeTrainingReport(cover, sizeof(cover)).pressed &&
              !decodeTrainingReport(outside, sizeof(outside)).valid, "cover is empty; invalid coordinates cannot score");
static_assert(trainingBeforeDeadline(10000, 9999) && !trainingBeforeDeadline(10000, 10000) && !trainingBeforeDeadline(10000, 10001), "tap deadline uses capture time");
static_assert(trainingBeforeDeadline(10, 0xFFFFFFFEu) && !trainingBeforeDeadline(10, 11), "tap deadline survives rollover");
constexpr bool legacyEmptyRearms() {
  T t;
  auto contact = decodeTrainingReport(down, sizeof(down));
  auto empty = decodeTrainingReport(stale, sizeof(stale));
  for (uint32_t n = 0; n < 100; ++n) {
    if (t.update(contact.pressed, contact.x, contact.y, n * 200) != T::HIT) return false;
    if (t.update(empty.pressed, empty.x, empty.y, n * 200 + 100) != T::NONE) return false;
  }
  return true;
}
constexpr bool manyContactReportsAreNotManyTaps() {
  T t;
  auto contact = decodeTrainingReport(down, sizeof(down));
  if (t.update(contact.pressed, contact.x, contact.y, 0) != T::HIT) return false;
  for (uint32_t n = 20; n <= 10000; n += 20)
    if (t.update(contact.pressed, contact.x, contact.y, n) != T::NONE) return false;
  return true;
}
constexpr uint8_t secondActive[] = {0x10, 0, 0, 0, 0, 2, 0xAB, 0x06, 0x0E, 0x0E, 0x99, 0};
constexpr uint8_t zeroHeaderNoTrailer[] = {0, 0, 0, 0, 0, 0, 0};
constexpr bool emptyClearsReleaseGuard() {
  T t; t.reset(true);
  auto empty = decodeTrainingReport(stale, sizeof(stale));
  return t.update(empty.pressed, 0, 0, 0) == T::NONE && !t.waitingRelease() &&
         t.update(true, 233, 233, 100) == T::HIT;
}
static_assert(legacyEmptyRearms(), "100 contact/ACK-empty cycles yield 100 hits, not one latched contact");
static_assert(manyContactReportsAreNotManyTaps(), "0x06 is contact, NOT a unique DOWN or per-IRQ strike");
static_assert(decodeTrainingReport(secondActive, sizeof(secondActive)).pressed &&
              decodeTrainingReport(secondActive, sizeof(secondActive)).x == 233,
              "choose first ACTIVE point like SensorLib, not first raw record");
static_assert(decodeTrainingReport(zeroHeaderNoTrailer, sizeof(zeroHeaderNoTrailer)).valid &&
              !decodeTrainingReport(zeroHeaderNoTrailer, sizeof(zeroHeaderNoTrailer)).pressed,
              "legacy empty packet without trailer must not latch previous contact");
static_assert(emptyClearsReleaseGuard(), "legacy empty sample clears a held-exit guard");
static_assert(T::POLL_MS == 20, "keep proven cadence, not four-times-faster ACK traffic");
}  // namespace
