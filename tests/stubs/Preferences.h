#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

class Preferences {
public:
  void begin(const char *, bool) {}
  void clear() {
    values.clear();
    blobs.clear();
  }

  bool isKey(const char *key) const {
    std::string k(key);
    return values.count(k) || blobs.count(k);
  }

  void putBool(const char *key, bool value) { values[key] = value ? "1" : "0"; }
  bool getBool(const char *key, bool fallback = false) const {
    return getValue(key, fallback ? "1" : "0") == "1";
  }

  void putUChar(const char *key, uint8_t value) { values[key] = std::to_string(value); }
  uint8_t getUChar(const char *key, uint8_t fallback = 0) const {
    return static_cast<uint8_t>(std::stoul(getValue(key, std::to_string(fallback))));
  }

  void putUInt(const char *key, uint32_t value) { values[key] = std::to_string(value); }
  uint32_t getUInt(const char *key, uint32_t fallback = 0) const {
    return static_cast<uint32_t>(std::stoul(getValue(key, std::to_string(fallback))));
  }

  void putShort(const char *key, int16_t value) { values[key] = std::to_string(value); }
  int16_t getShort(const char *key, int16_t fallback = 0) const {
    return static_cast<int16_t>(std::stoi(getValue(key, std::to_string(fallback))));
  }

  void putUShort(const char *key, uint16_t value) { values[key] = std::to_string(value); }
  uint16_t getUShort(const char *key, uint16_t fallback = 0) const {
    return static_cast<uint16_t>(std::stoul(getValue(key, std::to_string(fallback))));
  }

  int8_t getChar(const char *key, int8_t fallback = 0) const {
    return static_cast<int8_t>(std::stoi(getValue(key, std::to_string(fallback))));
  }

  void putString(const char *key, const char *value) { values[key] = value ? value : ""; }
  void getString(const char *key, char *out, size_t len) const {
    if (len == 0) return;
    std::string value = getValue(key, "");
    std::strncpy(out, value.c_str(), len - 1);
    out[len - 1] = '\0';
  }

  void putBytes(const char *key, const void *data, size_t len) {
    const auto *bytes = static_cast<const uint8_t *>(data);
    blobs[key] = std::vector<uint8_t>(bytes, bytes + len);
  }

  size_t getBytes(const char *key, void *out, size_t len) const {
    auto it = blobs.find(key);
    if (it == blobs.end()) {
      std::memset(out, 0, len);
      return 0;
    }
    size_t n = std::min(len, it->second.size());
    std::memcpy(out, it->second.data(), n);
    if (n < len) std::memset(static_cast<uint8_t *>(out) + n, 0, len - n);
    return n;
  }

private:
  std::string getValue(const char *key, const std::string &fallback) const {
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
  }

  std::unordered_map<std::string, std::string> values;
  std::unordered_map<std::string, std::vector<uint8_t>> blobs;
};
