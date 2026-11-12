#include "trainingcapture.h"
#if BOARD_AMOLED_175C
bool TrainingCapture::begin(Reader reader) {
  reader_ = reader;
  events_ = xQueueCreate(32, sizeof(TrainingEvent));
  done_ = xSemaphoreCreateBinary();
  if (events_ && done_ &&
      xTaskCreate(worker, "sackTouch", 4096, this, 2, &task_) == pdPASS) return true;
  if (events_) vQueueDelete(events_);
  if (done_) vSemaphoreDelete(done_);
  events_ = nullptr; done_ = nullptr; task_ = nullptr; reader_ = nullptr;
  return false;
}
void TrainingCapture::reset(bool requireRelease) {
  endDraw();
  touch_.reset(requireRelease);
  if (events_) xQueueReset(events_);
  lastPoll_ = millis() - TrainingTouch::POLL_MS;
  dropped_ = 0;
  traceRead_ = traceWrite_ = traceCount_ = 0;
  traceDropped_ = 0;
}
void TrainingCapture::poll() {
  uint32_t now = millis();
  if (!reader_ || now - lastPoll_ < TrainingTouch::POLL_MS) return;
  lastPoll_ = now;
  TrainingReport report = reader_();
  auto action = report.valid ? touch_.update(report.pressed, report.x, report.y, now) : TrainingTouch::NONE;
  if (tracing_ && report.attempted) {
    if (traceCount_ == 16) ++traceDropped_;
    else {
      traces_[traceWrite_] = {now, report, action};
      traceWrite_ = (traceWrite_ + 1) % 16; ++traceCount_;
    }
  }
  TrainingEvent event{action, now};
  if (event.action != TrainingTouch::NONE && xQueueSend(events_, &event, 0) != pdTRUE) ++dropped_;
}
bool TrainingCapture::nextTrace(TrainingTrace &trace) {
  if (!traceCount_) return false;
  trace = traces_[traceRead_];
  traceRead_ = (traceRead_ + 1) % 16; --traceCount_;
  return true;
}
bool TrainingCapture::next(TrainingEvent &event) {
  return events_ && xQueueReceive(events_, &event, 0) == pdTRUE;
}
void TrainingCapture::startDraw() {
  if (!task_) return;
  drawing_.store(true);
  xTaskNotifyGive(task_);
}
void TrainingCapture::endDraw() {
  if (!task_ || !drawing_.exchange(false)) return;
  xSemaphoreTake(done_, portMAX_DELAY);
}
void TrainingCapture::worker(void *arg) {
  auto &c = *static_cast<TrainingCapture *>(arg);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (c.drawing_.load()) {
      c.poll();
      vTaskDelay(1);  // 1 scheduler tick; poll() rate-limits I2C to the legacy 20 ms cadence
    }
    xSemaphoreGive(c.done_);
  }
}
#endif
