#pragma once
#include "board.h"
#include "trainingtouch.h"
#if BOARD_AMOLED_175C
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>

struct TrainingTrace {
  uint32_t at;
  TrainingReport report;
  TrainingTouch::Action action;
};

struct TrainingEvent {
  TrainingTouch::Action action;
  uint32_t at;
};

// Only the loop owns the UI/pet. During the I2C-free part of renderSack(),
// this worker samples touch and queues actions while QSPI drawing/flush blocks.
// endDraw() joins it BEFORE the loop can use Wire, the driver, or the policy.
// No cross-task Wire/RX-buffer sharing; no I2C or game logic in the touch ISR.
class TrainingCapture {
 public:
  using Reader = TrainingReport (*)();
  bool begin(Reader reader);
  void reset(bool requireRelease = false);
  void poll();                       // loop, or worker during drawing; never both
  bool next(TrainingEvent &event);
  void setTracing(bool enabled) { tracing_ = enabled; traceRead_ = traceWrite_ = traceCount_ = 0; traceDropped_ = 0; }
  bool tracing() const { return tracing_; }
  bool nextTrace(TrainingTrace &trace);
  uint32_t traceDropped() const { return traceDropped_; }
  bool down() const { return touch_.down(); }
  bool waitingRelease() const { return touch_.waitingRelease(); }
  uint32_t exitHeldMs(uint32_t now) const { return touch_.exitHeldMs(now); }
  uint32_t dropped() const { return dropped_; }  // read only after endDraw()
  class Frame {
   public:
    explicit Frame(TrainingCapture &c, bool enabled = true) : capture_(c), enabled_(enabled) {
      if (enabled_) capture_.startDraw();
    }
    ~Frame() { if (enabled_) capture_.endDraw(); }
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
   private:
    TrainingCapture &capture_;
    bool enabled_;
  };
 private:
  static void worker(void *arg);
  void startDraw();
  void endDraw();
  Reader reader_ = nullptr;
  TaskHandle_t task_ = nullptr;
  QueueHandle_t events_ = nullptr;
  SemaphoreHandle_t done_ = nullptr;
  std::atomic<bool> drawing_{false};
  TrainingTouch touch_;
  uint32_t lastPoll_ = 0, dropped_ = 0;
  // Exclusive-owner rule is the same as for touch_: loop OR joined worker.
  // Disabled at boot; never print from the ISR/worker or block on logging.
  bool tracing_ = false;
  TrainingTrace traces_[16];
  uint8_t traceRead_ = 0, traceWrite_ = 0, traceCount_ = 0;
  uint32_t traceDropped_ = 0;
};
#endif
