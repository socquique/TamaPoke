#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using String = std::string;

extern uint32_t gMockMillis;

inline uint32_t millis() {
  return gMockMillis;
}

inline void delay(uint32_t ms) {
  gMockMillis += ms;
}

inline long random(long max) {
  if (max <= 0) return 0;
  return 0;
}

inline long random(long min, long max) {
  if (max <= min) return min;
  return min + random(max - min);
}

template <typename T>
inline T min(T a, T b) {
  return std::min(a, b);
}

template <typename T>
inline T max(T a, T b) {
  return std::max(a, b);
}

class SerialMock {
public:
  template <typename... Args>
  void printf(const char *, Args...) {}

  void println(const char *) {}
};

extern SerialMock Serial;
