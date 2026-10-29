// The detail tests' shared pieces: an FNV-1a 64 over doubles' bits (a golden row stays short
// however many vertices a pass writes) and the golden file's read-or-write.
//
// ge:summary Detail tests' helpers: a bits hash and the golden read-or-write
// ge:keywords test detail golden fnv hash bits
// ge:skill detail testing
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "detail/vec.hpp"
#include "golden.hpp"

namespace ge::test {

struct BitsHash {
  uint64_t h = 1469598103934665603ull;
  void add(double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int i = 0; i < 8; ++i) {
      h ^= (u >> (i * 8)) & 0xff;
      h *= 1099511628211ull;
    }
  }
  void add(const detail::Vec3& v) {
    add(v.x);
    add(v.y);
    add(v.z);
  }
};

inline bool golden_write_set() {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, "GE_GOLDEN_WRITE") == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv("GE_GOLDEN_WRITE") != nullptr;
#endif
}

// `got` against the golden at `rel`, or written there under GE_GOLDEN_WRITE.
inline void check_golden(const char* rel, const std::vector<std::vector<uint64_t>>& got,
                         const char* header, const std::string& hint) {
  if (golden_write_set()) {
    std::ofstream out(root_path(rel));
    out << "# " << header << "\n";
    for (const auto& row : got) {
      std::string line;
      for (uint64_t w : row) {
        char b[20];
        std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(w));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  const auto want = golden_rows(rel);
  CHECK_MSG(!want.empty(), std::string(rel) + " is empty; write it with GE_GOLDEN_WRITE=1");
  CHECK_MSG(got == want, std::string(rel) + " moved: " + hint);
}

}  // namespace ge::test
