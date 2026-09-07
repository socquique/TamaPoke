// Micro framework de tests: sin dependencias, un solo binario.
#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

struct TestCase {
  std::string suite;
  std::string name;
  std::function<void()> fn;
  bool knownIssue;   // documenta un fallo real del firmware: no rompe la suite
  std::string note;
};

std::vector<TestCase> &testRegistry();

struct TestRegistrar {
  TestRegistrar(const char *suite, const char *name, std::function<void()> fn,
                bool knownIssue = false, const char *note = "") {
    TestCase tc;
    tc.suite = suite;
    tc.name = name;
    tc.fn = fn;
    tc.knownIssue = knownIssue;
    tc.note = note;
    testRegistry().push_back(tc);
  }
};

#define TEST(suite, name)                                                        \
  static void suite##_##name##_body();                                           \
  static TestRegistrar suite##_##name##_reg(#suite, #name, suite##_##name##_body); \
  static void suite##_##name##_body()

// Test que describe el comportamiento CORRECTO de algo que hoy falla.
// No rompe la suite: se informa aparte (y avisa si algun dia pasa a verde).
#define TEST_KNOWN_ISSUE(suite, name, note)                                      \
  static void suite##_##name##_body();                                           \
  static TestRegistrar suite##_##name##_reg(#suite, #name, suite##_##name##_body, \
                                            true, note);                         \
  static void suite##_##name##_body()

void testFail(const char *file, int line, const std::string &msg);
extern int gAssertCount;

static inline std::string testStr(uint8_t v) { return std::to_string((int)v); }
static inline std::string testStr(int8_t v) { return std::to_string((int)v); }
static inline std::string testStr(bool v) { return v ? "true" : "false"; }
static inline std::string testStr(const char *v) { return v ? std::string("\"") + v + "\"" : "(null)"; }
static inline std::string testStr(char *v) { return testStr((const char *)v); }
template <class T>
static inline std::string testStr(const T &v) {
  std::ostringstream os;
  os << v;
  return os.str();
}

#define CHECK(cond)                                                              \
  do {                                                                           \
    gAssertCount++;                                                              \
    if (!(cond)) testFail(__FILE__, __LINE__, "CHECK(" #cond ")");               \
  } while (0)

#define CHECK_MSG(cond, msg)                                                     \
  do {                                                                           \
    gAssertCount++;                                                              \
    if (!(cond)) testFail(__FILE__, __LINE__, std::string("CHECK(" #cond ") ") + (msg)); \
  } while (0)

#define CHECK_EQ(a, b)                                                           \
  do {                                                                           \
    gAssertCount++;                                                              \
    auto _va = (a);                                                              \
    auto _vb = (b);                                                              \
    if (!(_va == _vb))                                                           \
      testFail(__FILE__, __LINE__, std::string(#a " == " #b " -> ") + testStr(_va) + " vs " + testStr(_vb)); \
  } while (0)

#define CHECK_RANGE(v, lo, hi)                                                   \
  do {                                                                           \
    gAssertCount++;                                                              \
    auto _v = (v);                                                               \
    if (!(_v >= (lo) && _v <= (hi)))                                             \
      testFail(__FILE__, __LINE__, std::string(#v " in [" #lo "," #hi "] -> ") + testStr(_v)); \
  } while (0)

#define CHECK_STREQ(a, b)                                                        \
  do {                                                                           \
    gAssertCount++;                                                              \
    const char *_sa = (a);                                                       \
    const char *_sb = (b);                                                       \
    if (!_sa || !_sb || strcmp(_sa, _sb) != 0)                                   \
      testFail(__FILE__, __LINE__, std::string(#a " == " #b " -> ") + testStr(_sa) + " vs " + testStr(_sb)); \
  } while (0)
