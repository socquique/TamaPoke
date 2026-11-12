#include "screenlock.h"

// Exercise the actual pure state machines in every Arduino build, including
// when no host C++ compiler is installed. No test code executes on the board.
namespace {
using B = PwrButton;
constexpr bool edge(B &b, bool down, uint32_t t) {
  b.update(down, t);
  return b.update(down, t + B::DEBOUNCE_MS);
}
constexpr bool tap(B &b, uint32_t t, uint32_t held = 100) {
  edge(b, true, t);
  return edge(b, false, t + held);
}
constexpr bool triple() {
  B b; b.begin(false, 0);
  return !tap(b, 100) && !tap(b, 300) && tap(b, 500) &&
         !b.active() && !b.update(false, 1200);
}
constexpr bool noSingleDouble() {
  B b; b.begin(false, 0);
  if (tap(b, 100) || b.update(false, 551) || b.active()) return false;
  return !tap(b, 600) && !tap(b, 800) && !b.update(false, 1251) && !b.active();
}
constexpr bool thirdReleaseOnly() {
  B b; b.begin(false, 0); tap(b, 100); tap(b, 300);
  return !edge(b, true, 500) && !b.update(false, 600) &&
         !b.update(false, 624) && b.update(false, 625) && !b.update(false, 626);
}
constexpr bool exactGap() {
  B b; b.begin(false, 0);
  return !tap(b, 100) && !tap(b, 550) && tap(b, 1000);
}
constexpr bool lateSecond() {
  B b; b.begin(false, 0);
  return !tap(b, 100) && !tap(b, 551) && !tap(b, 800) && tap(b, 1000);
}
constexpr bool lateThird() {
  B b; b.begin(false, 0);
  return !tap(b, 100) && !tap(b, 300) && !tap(b, 751) &&
         !b.update(false, 1202) && !b.active();
}
constexpr bool heldAtBegin() {
  B b; b.begin(true, 0);
  if (b.update(true, 2000) || edge(b, false, 2100)) return false;
  return !tap(b, 2200) && !tap(b, 2400) && tap(b, 2600);
}
constexpr bool longFirst() {
  B b; b.begin(false, 0);
  return !tap(b, 100, 600) && !tap(b, 800) && !tap(b, 1000) &&
         !b.update(false, 1451) && !b.active();
}
constexpr bool longSecond() {
  B b; b.begin(false, 0); tap(b, 100); edge(b, true, 300);
  return !b.update(true, 900) && !edge(b, false, 1000) &&
         !tap(b, 1100) && !tap(b, 1300) && !b.update(false, 1751) && !b.active();
}
constexpr bool longThird() {
  B b; b.begin(false, 0); tap(b, 100); tap(b, 300);
  return !tap(b, 500, 600) && !b.update(false, 1600) && !b.active();
}
constexpr bool belowTapLimit() {
  B b; b.begin(false, 0);
  return !tap(b, 100, 599) && !tap(b, 800, 599) && tap(b, 1500, 599);
}
constexpr bool longLateRelease() {
  B b; b.begin(false, 0); tap(b, 100); tap(b, 300); edge(b, true, 500);
  // Even if there was no sample during the hold, the release must cancel it.
  return !edge(b, false, 1600) && !b.update(false, 2200) && !b.active();
}
constexpr bool bounce() {
  B b; b.begin(false, 0);
  b.update(true, 100); b.update(false, 110); b.update(true, 120);
  b.update(true, 144); b.update(true, 145);
  b.update(false, 200); b.update(true, 210); b.update(false, 220);
  if (b.update(false, 244) || b.update(false, 245)) return false;
  return !tap(b, 300) && tap(b, 500);
}
constexpr bool tinyGlitch() {
  B b; b.begin(false, 0);
  b.update(true, 100); b.update(false, 124); b.update(false, 150);
  return !tap(b, 200) && !tap(b, 400) && !b.update(false, 900) && !b.active();
}
constexpr bool wrapTriple() {
  B b; uint32_t t = 0xFFFFFF00u; b.begin(false, t);
  return !tap(b, t + 10) && !tap(b, t + 210) && tap(b, t + 410);
}
constexpr bool wrapGap() {
  B b; uint32_t t = 0xFFFFFF00u; b.begin(false, t); tap(b, t + 10);
  return !b.update(false, t + 460) && b.active() &&
         !b.update(false, t + 461) && !b.active();
}
constexpr bool repeatedTriples() {
  B b; b.begin(false, 0);
  return !tap(b, 100) && !tap(b, 300) && tap(b, 500) &&
         !tap(b, 700) && !tap(b, 900) && tap(b, 1100);
}
constexpr bool hintWaitsForFrame() {
  ScreenLock l;
  if (l.locked() || l.blank(10000) || !l.toggle() || !l.locked() || l.blank(10000)) return false;
  l.frameShown(10000);
  l.frameShown(10500);  // repeated renders do not restart the hint timer
  return !l.blank(10599) && l.blank(10600);
}
constexpr bool unlockClearsBlank() {
  ScreenLock l; l.toggle(); l.frameShown(100);
  return l.blank(700) && !l.toggle() && !l.locked() && !l.blank(10000);
}
constexpr bool retoggleDuringHint() {
  ScreenLock l; l.toggle(); l.frameShown(100); l.toggle(); l.toggle();
  if (l.blank(10000)) return false;
  l.frameShown(10000);
  return !l.blank(10599) && l.blank(10600);
}
constexpr bool wrapHint() {
  ScreenLock l; uint32_t t = 0xFFFFFF00u; l.toggle(); l.frameShown(t);
  return !l.blank(t + 599) && l.blank(t + 600) && !l.toggle() && !l.blank(t + 1000);
}
constexpr bool blankStaysLatched() {
  ScreenLock l; l.toggle(); l.frameShown(100);
  return l.blank(700) && l.blank(100) && l.blank(699);  // a whole wrap later
}
static_assert(blankStaysLatched(), "a long lock must remain blank through later full wraps");
static_assert(triple(), "exactly three completed taps fire once");
static_assert(noSingleDouble(), "single and double PWR presses must do nothing");
static_assert(thirdReleaseOnly(), "toggle only after the third debounced release");
static_assert(exactGap(), "350ms next-press edge accepted through debounce");
static_assert(lateSecond(), "late second starts a new triple");
static_assert(lateThird(), "late third must not toggle");
static_assert(heldAtBegin(), "ignore a PWR key held during initialisation");
static_assert(longFirst(), "600ms is not a short PWR tap");
static_assert(longSecond(), "held second cancels the whole triple");
static_assert(longThird(), "held third must not toggle");
static_assert(belowTapLimit(), "599ms taps are valid");
static_assert(longLateRelease(), "late polling must not count a long release");
static_assert(bounce(), "debounce both edges without counting bounce taps");
static_assert(tinyGlitch(), "pulses shorter than debounce do not count");
static_assert(wrapTriple(), "triple survives millis rollover");
static_assert(wrapGap(), "gap expiry survives millis rollover");
static_assert(repeatedTriples(), "successive triples toggle independently");
static_assert(hintWaitsForFrame(), "boot unlocked; wait 600ms after first flushed hint");
static_assert(unlockClearsBlank(), "unlock clears blank immediately");
static_assert(retoggleDuringHint(), "a new lock gets its own first-frame timer");
static_assert(wrapHint(), "hint timer survives millis rollover");
}  // namespace
