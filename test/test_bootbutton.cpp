#include "framework.h"
#include "../bootbutton.h"

using B = BootButton;
static B::Action edge(B &b, bool down, uint32_t t) {
  CHECK_EQ(b.update(down, t), B::NONE);
  return b.update(down, t + B::DEBOUNCE_MS);
}

TEST(BootButton, SingleWaitsForWindow) {
  B b; b.begin(false, 0);
  CHECK_EQ(edge(b, true, 100), B::NONE);
  CHECK_EQ(edge(b, false, 200), B::NONE);
  CHECK_EQ(b.update(false, 549), B::NONE);
  CHECK_EQ(b.update(false, 550), B::SINGLE);
  CHECK_EQ(b.update(false, 900), B::NONE);
  CHECK(!b.active());
}
TEST(BootButton, DoubleSuppressesSingle) {
  B b; b.begin(false, 0);
  CHECK_EQ(edge(b, true, 100), B::NONE);
  CHECK_EQ(edge(b, false, 200), B::NONE);
  CHECK_EQ(edge(b, true, 400), B::NONE);
  CHECK_EQ(edge(b, false, 500), B::DOUBLE);
  CHECK_EQ(b.update(false, 900), B::NONE);
}
TEST(BootButton, SecondEdgeAtWindowBoundary) {
  B b; b.begin(false, 0);
  edge(b, true, 100); edge(b, false, 200);
  CHECK_EQ(b.update(true, 550), B::NONE);
  CHECK_EQ(b.update(true, 575), B::NONE);
  CHECK_EQ(edge(b, false, 650), B::DOUBLE);
  CHECK_EQ(b.update(false, 1200), B::NONE);
}
TEST(BootButton, LateSecondPressIsTwoSingles) {
  B b; b.begin(false, 0);
  edge(b, true, 100); edge(b, false, 200);
  CHECK_EQ(b.update(true, 551), B::NONE);
  CHECK_EQ(b.update(true, 576), B::SINGLE);
  CHECK_EQ(edge(b, false, 650), B::NONE);
  CHECK_EQ(b.update(false, 1000), B::SINGLE);
}
TEST(BootButton, LongFiresOnceWithoutSingle) {
  B b; b.begin(false, 0);
  edge(b, true, 100);
  CHECK_EQ(b.update(true, 1599), B::NONE);
  CHECK_EQ(b.update(true, 1600), B::LONG);
  CHECK_EQ(b.update(true, 3000), B::NONE);
  CHECK_EQ(edge(b, false, 3100), B::NONE);
  CHECK_EQ(b.update(false, 4000), B::NONE);
}
TEST(BootButton, LongSecondPressWinsOverDoubleAndSingle) {
  B b; b.begin(false, 0);
  edge(b, true, 100); edge(b, false, 200); edge(b, true, 400);
  CHECK_EQ(b.update(true, 600), B::NONE);
  CHECK_EQ(b.update(true, 1900), B::LONG);
  CHECK_EQ(edge(b, false, 2000), B::NONE);
  CHECK_EQ(b.update(false, 3000), B::NONE);
}
TEST(BootButton, ReleaseAfterLongWhenLoopWasBusy) {
  B b; b.begin(false, 0);
  edge(b, true, 100);
  CHECK_EQ(edge(b, false, 1700), B::LONG);
  CHECK_EQ(b.update(false, 2500), B::NONE);
}
TEST(BootButton, DebouncesBothEdges) {
  B b; b.begin(false, 0);
  CHECK_EQ(b.update(true, 100), B::NONE);
  CHECK_EQ(b.update(false, 110), B::NONE);
  CHECK_EQ(b.update(true, 120), B::NONE);
  CHECK_EQ(b.update(true, 144), B::NONE);
  CHECK_EQ(b.update(true, 145), B::NONE);
  CHECK_EQ(b.update(false, 200), B::NONE);
  CHECK_EQ(b.update(true, 210), B::NONE);
  CHECK_EQ(b.update(false, 220), B::NONE);
  CHECK_EQ(b.update(false, 245), B::NONE);
  CHECK_EQ(b.update(false, 569), B::NONE);
  CHECK_EQ(b.update(false, 570), B::SINGLE);
}
TEST(BootButton, HeldAtInitialisationIsIgnored) {
  B b; b.begin(true, 0);
  CHECK_EQ(b.update(true, 2000), B::NONE);
  CHECK_EQ(edge(b, false, 2200), B::NONE);
  CHECK_EQ(b.update(false, 3000), B::NONE);
  CHECK_EQ(edge(b, true, 3100), B::NONE);
  CHECK_EQ(edge(b, false, 3200), B::NONE);
  CHECK_EQ(b.update(false, 3550), B::SINGLE);
}
TEST(BootButton, MillisWrapSingleAndLong) {
  B b; uint32_t base = 0xFFFFFF00u; b.begin(false, base);
  edge(b, true, base + 10); edge(b, false, base + 100);
  CHECK_EQ(b.update(false, base + 449), B::NONE);
  CHECK_EQ(b.update(false, base + 450), B::SINGLE);
  edge(b, true, base + 500);
  CHECK_EQ(b.update(true, base + 1999), B::NONE);
  CHECK_EQ(b.update(true, base + 2000), B::LONG);
}
TEST(BootButton, MillisWrapDouble) {
  B b; uint32_t base = 0xFFFFFF00u; b.begin(false, base);
  edge(b, true, base + 10); edge(b, false, base + 100);
  edge(b, true, base + 400);
  CHECK_EQ(edge(b, false, base + 500), B::DOUBLE);
  CHECK_EQ(b.update(false, base + 1000), B::NONE);
}
TEST(BootButton, ShortHoldBelowThresholdIsSingle) {
  B b; b.begin(false, 0);
  edge(b, true, 100);
  CHECK_EQ(edge(b, false, 1599), B::NONE);
  CHECK_EQ(b.update(false, 1949), B::SINGLE);
}
