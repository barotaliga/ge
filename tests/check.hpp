// THE TEST HARNESS: no dependency, one header. A test file is a list of
// TEST(name) blocks; CHECK records a failure and carries on, so one run
// names every broken case. Each test file is its own executable, registered
// with CTest by tests/CMakeLists.txt; exit code 1 on any failure.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace ge::test {

struct Case {
  const char* name;
  void (*fn)();
  Case* next;
};

inline Case*& cases() {
  static Case* head = nullptr;
  return head;
}
inline int& failures() {
  static int n = 0;
  return n;
}

struct Register {
  Case node;
  Register(const char* name, void (*fn)()) : node{name, fn, cases()} { cases() = &node; }
};

inline uint64_t bits(double v) {
  uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

inline void fail(const char* file, int line, const std::string& what) {
  ++failures();
  std::fprintf(stderr, "%s:%d: FAIL %s\n", file, line, what.c_str());
}

// The project root, so a test can open tests/golden/* from any build dir.
inline std::string root_path(const char* rel) { return std::string(GE_SOURCE_DIR) + "/" + rel; }

}  // namespace ge::test

#define GE_CAT2(a, b) a##b
#define GE_CAT(a, b) GE_CAT2(a, b)
#define TEST(name)                                                             \
  static void GE_CAT(test_, name)();                                           \
  static ::ge::test::Register GE_CAT(reg_, name)(#name, &GE_CAT(test_, name)); \
  static void GE_CAT(test_, name)()

#define CHECK(cond) ((cond) ? (void)0 : ::ge::test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"))

// Bit equality of two doubles — the determinism contract is bits, not closeness.
#define CHECK_BITS(got, want)                                                                \
  do {                                                                                       \
    const double g_ = (got), w_ = (want);                                                    \
    if (::ge::test::bits(g_) != ::ge::test::bits(w_)) {                                      \
      char m_[160];                                                                          \
      std::snprintf(m_, sizeof m_, "%s: got %.17g (%016llx) want %.17g (%016llx)", #got, g_, \
                    (unsigned long long)::ge::test::bits(g_), w_,                            \
                    (unsigned long long)::ge::test::bits(w_));                               \
      ::ge::test::fail(__FILE__, __LINE__, m_);                                              \
    }                                                                                        \
  } while (0)

// CHECK_BITS as JS can observe it: any NaN equals any NaN, because JS has one NaN and V8 does
// not keep NaN payloads or signs stable. Use it for results JS code produced.
#define CHECK_JS_BITS(got, want)                                                             \
  do {                                                                                       \
    const double g_ = (got), w_ = (want);                                                    \
    if (!(g_ != g_ && w_ != w_) && ::ge::test::bits(g_) != ::ge::test::bits(w_)) {           \
      char m_[160];                                                                          \
      std::snprintf(m_, sizeof m_, "%s: got %.17g (%016llx) want %.17g (%016llx)", #got, g_, \
                    (unsigned long long)::ge::test::bits(g_), w_,                            \
                    (unsigned long long)::ge::test::bits(w_));                               \
      ::ge::test::fail(__FILE__, __LINE__, m_);                                              \
    }                                                                                        \
  } while (0)
