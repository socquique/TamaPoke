#include "framework.h"
#include "../trainingtouch.h"
using T = TrainingTouch;
TEST(TrainingTouch, OneHundredRapidTapsNoRepeatWhileHeld) {
  T t;
  for (uint32_t n = 0; n < 100; ++n) {
    CHECK_EQ(t.update(true, 233, 120, n * 30), T::HIT);
    CHECK_EQ(t.update(true, 234, 120, n * 30 + 5), T::NONE);
    CHECK_EQ(t.update(false, 0, 0, n * 30 + 15), T::NONE);
  }
}
TEST(TrainingTouch, TopAndBroadScreenAreHits) {
  for (auto y : {30, 80, 160, 240, 320, 370}) {
    T t; CHECK_EQ(t.update(true, 233, y, 0), T::HIT);
  }
  CHECK(!T::onPanel(0, 0)); CHECK(!T::onPanel(467, 233));
  CHECK(T::onPanel(T::EXIT_X, T::EXIT_Y + T::EXIT_H));
}
TEST(TrainingTouch, ExitRequiresAContinuousOneSecondHold) {
  T t;
  for (uint32_t n = 0; n < 1000; n += 25)
    CHECK_EQ(t.update(true, 233, 405, n), T::NONE);
  CHECK_EQ(t.update(true, 233, 405, 999), T::NONE);
  CHECK_EQ(t.update(true, 233, 405, 1000), T::EXIT);
  CHECK(t.waitingRelease());
  CHECK_EQ(t.update(true, 233, 405, 2000), T::NONE);
  CHECK_EQ(t.update(false, 0, 0, 2100), T::NONE);
  CHECK(!t.waitingRelease());
}
TEST(TrainingTouch, QuickTapsOnExitDoNotExitOrScore) {
  T t;
  for (uint32_t n = 0; n < 100; ++n) {
    CHECK_EQ(t.update(true, 233, 405, n * 50), T::NONE);
    CHECK_EQ(t.update(false, 0, 0, n * 50 + 20), T::NONE);
  }
}
TEST(TrainingTouch, MovingOrMissingReportsCancelsExit) {
  T t; t.update(true, 233, 405, 0); t.update(true, 233, 370, 25);
  for (uint32_t n = 50; n <= 1500; n += 25)
    CHECK_EQ(t.update(true, 233, 405, n), T::NONE);
  t.reset(); t.update(true, 233, 405, 0);
  CHECK_EQ(t.update(true, 233, 405, 1000), T::NONE);
  t.reset(); CHECK_EQ(t.update(true, 233, 233, 0), T::HIT);
  for (uint32_t n = 25; n <= 1500; n += 25)
    CHECK_EQ(t.update(true, 233, 405, n), T::NONE);
}
TEST(TrainingTouch, LockAndExitSwallowHeldContact) {
  T t; t.update(true, 233, 405, 0); t.reset(true);
  for (uint32_t n = 0; n < 2000; n += 25)
    CHECK_EQ(t.update(true, 233, 405, n), T::NONE);
  CHECK(t.waitingRelease());
  t.update(false, 0, 0, 2000); CHECK(!t.waitingRelease());
  CHECK_EQ(t.update(true, 233, 233, 2050), T::HIT);
}
TEST(TrainingTouch, DecoderRestoresLegacyEmptyRearming) {
  const uint8_t down[] = {0x06, 0x0E, 0x1B, 0x94, 0, 1, 0xAB};
  const uint8_t ack[] = {0xAB, 0, 0, 0, 0, 0, 0xAB};
  const uint8_t up[] = {0, 0, 0, 0, 0, 0, 0xAB};
  CHECK(decodeTrainingReport(ack, 7).valid);
  CHECK(!decodeTrainingReport(ack, 7).pressed);
  CHECK_EQ(decodeTrainingReport(ack, 7).kind, TrainingReport::EMPTY);
  CHECK(!decodeTrainingReport(down, 6).valid);
  CHECK(decodeTrainingReport(down, 7).pressed);
  CHECK_EQ(decodeTrainingReport(down, 7).x, 233);
  CHECK_EQ(decodeTrainingReport(down, 7).y, 30);
  CHECK(decodeTrainingReport(up, 7).valid);
  CHECK(!decodeTrainingReport(up, 7).pressed);
}
TEST(TrainingTouch, RolloverAndCaptureTimeDeadline) {
  T t; uint32_t start = 0xFFFFFF00u;
  for (uint32_t n = 0; n < 1000; n += 25)
    CHECK_EQ(t.update(true, 233, 405, start + n), T::NONE);
  CHECK_EQ(t.update(true, 233, 405, start + 1000), T::EXIT);
  CHECK(trainingBeforeDeadline(10, 0xFFFFFFFEu));
  CHECK(!trainingBeforeDeadline(10, 10)); CHECK(!trainingBeforeDeadline(10, 11));
}

TEST(TrainingTouch, ContactEmptyCyclesRecoverEveryTap) {
  const uint8_t down[] = {0x06, 0x0E, 0x0E, 0x99, 0, 1, 0xAB};
  const uint8_t empty[] = {0xAB, 0, 0, 0, 0, 0, 0xAB};
  T t;
  for (uint32_t n = 0; n < 100; ++n) {
    auto r = decodeTrainingReport(down, 7);
    CHECK_EQ(t.update(r.pressed, r.x, r.y, n * 200), T::HIT);
    r = decodeTrainingReport(empty, 7);
    CHECK_EQ(t.update(r.pressed, r.x, r.y, n * 200 + 100), T::NONE);
  }
}
TEST(TrainingTouch, ContactFramesAndTrackIdsAreNotTapCounters) {
  T t;
  CHECK_EQ(t.update(true, 233, 233, 0), T::HIT);
  for (uint32_t n = 20; n <= 10000; n += 20)
    CHECK_EQ(t.update(true, 233, 233, n), T::NONE);
  CHECK_EQ(T::POLL_MS, 20u);
}
TEST(TrainingTouch, SecondRecordCanBeFirstActiveFinger) {
  const uint8_t b[] = {0x10, 0, 0, 0, 0, 2, 0xAB, 0x06, 0x0E, 0x0E, 0x99, 0};
  auto r = decodeTrainingReport(b, sizeof(b));
  CHECK(r.valid); CHECK(r.pressed); CHECK_EQ(r.x, 233); CHECK_EQ(r.y, 233);
}
TEST(TrainingTouch, EmptyClearsHeldExitAndZeroPacketDoesNotLatch) {
  const uint8_t b[] = {0, 0, 0, 0, 0, 0, 0};
  auto r = decodeTrainingReport(b, sizeof(b));
  CHECK(r.valid); CHECK(!r.pressed);
  T t; t.reset(true); t.update(r.pressed, 0, 0, 0);
  CHECK(!t.waitingRelease()); CHECK_EQ(t.update(true, 233, 233, 100), T::HIT);
}
