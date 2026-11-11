#include "framework.h"
#include "../screenlock.h"
#include "../power.h"
#include "../bootbutton.h"

using B = PwrButton;
static bool edge(B &b, bool down, uint32_t t) {
  CHECK(!b.update(down, t));
  return b.update(down, t + B::DEBOUNCE_MS);
}
static bool tap(B &b, uint32_t t, uint32_t held = 100) {
  CHECK(!edge(b, true, t));
  return edge(b, false, t + held);
}
TEST(ScreenLock, TripleTogglesAndResetStartsUnlocked) {
  B b; b.begin(false, 0); ScreenLock l;
  CHECK(!l.locked());
  CHECK(!tap(b, 100)); CHECK(!tap(b, 300)); CHECK(tap(b, 500));
  CHECK(l.toggle()); CHECK(l.locked()); CHECK(!l.blank(10000));
  l.frameShown(10000); l.frameShown(10500);
  CHECK(!l.blank(10599)); CHECK(l.blank(10600));
  CHECK(l.blank(10000));  // stays blank even after an entire uint32_t wrap
  CHECK(!tap(b, 11000)); CHECK(!tap(b, 11200)); CHECK(tap(b, 11400));
  CHECK(!l.toggle()); CHECK(!l.blank(12000));
  ScreenLock rebooted; CHECK(!rebooted.locked());
}
TEST(ScreenLock, SingleDoubleAndHoldHaveNoAction) {
  B b; b.begin(false, 0);
  CHECK(!tap(b, 100)); CHECK(!b.update(false, 551)); CHECK(!b.active());
  CHECK(!tap(b, 600)); CHECK(!tap(b, 800));
  CHECK(!b.update(false, 1251)); CHECK(!b.active());
  CHECK(!tap(b, 1300)); CHECK(!tap(b, 1500, 600));
  CHECK(!tap(b, 2200)); CHECK(!tap(b, 2400));
  CHECK(!b.update(false, 2851)); CHECK(!b.active());
}
TEST(ScreenLock, LateAndBoundaryEdges) {
  B b; b.begin(false, 0);
  CHECK(!tap(b, 100)); CHECK(!tap(b, 550)); CHECK(tap(b, 1000));
  b.begin(false, 0);
  CHECK(!tap(b, 100)); CHECK(!tap(b, 300)); CHECK(!tap(b, 751));
  CHECK(!b.update(false, 1202)); CHECK(!b.active());
}
TEST(ScreenLock, PendingBootGestureIsDiscardedAndHeldKeyIgnored) {
  BootButton boot; boot.begin(false, 0);
  boot.update(true, 100); boot.update(true, 125);
  boot.update(false, 200); boot.update(false, 225);
  CHECK(boot.active());
  boot.begin(false, 300);  // same reset performed when locking/unlocking
  CHECK_EQ(boot.update(false, 1000), BootButton::NONE);
  boot.begin(true, 1100);  // held through unlock: release is not a mute/pair action
  CHECK_EQ(boot.update(true, 4000), BootButton::NONE);
  boot.update(false, 4100);
  CHECK_EQ(boot.update(false, 4125), BootButton::NONE);
  CHECK_EQ(boot.update(false, 5000), BootButton::NONE);
}
TEST(ScreenLock, BlankInhibitsRenderingBrightnessAndAmplifier) {
  ScreenLock l; l.toggle(); l.frameShown(100);
  PowerCtx c = {}; c.screenOff = l.blank(700); c.batPct = 80;
  CHECK_EQ(frameIntervalMs(c), 0u); CHECK_EQ(brightnessFor(c), 0);
  CHECK(!amplifierWanted(c)); CHECK(lightSleepAllowed(c));
  c.usb = true;
  CHECK_EQ(frameIntervalMs(c), 0u); CHECK_EQ(brightnessFor(c), 0);
  CHECK(!lightSleepAllowed(c));
}
TEST(ScreenLock, WrapAndRelockHint) {
  uint32_t t = 0xFFFFFF00u;
  B b; b.begin(false, t);
  CHECK(!tap(b, t + 10)); CHECK(!tap(b, t + 210)); CHECK(tap(b, t + 410));
  ScreenLock l; l.toggle(); l.frameShown(t);
  CHECK(!l.blank(t + 599)); CHECK(l.blank(t + 600));
  l.toggle(); l.toggle(); CHECK(!l.blank(t + 10000));
  l.frameShown(t + 10000);
  CHECK(!l.blank(t + 10599)); CHECK(l.blank(t + 10600));
}
