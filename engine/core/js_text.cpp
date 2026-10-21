// JS numbers as text: see js_text.hpp.
//
// ge:summary The exact decimal expansion and shortest round-trip behind js_text.hpp
// ge:keywords js toFixed toString
// ge:skill determinism agent-interface
// ge:test tests/core_js_test.cpp
#include "core/js_text.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ge::core::js {

namespace {

// The shortest digits of a finite x > 0 that read back as x, and its decimal exponent: x is
// 0.DIGITS × 10^exp10. `%.Ng` is the nearest N-digit decimal, so the first N that round-trips
// is the shortest and, among the shortest, the closest (what JS's Number::toString picks).
void shortest(double x, std::string& digits, int& exp10) {
  char buf[64];
  for (int n = 1; n <= 17; n++) {
    std::snprintf(buf, sizeof buf, "%.*e", n - 1, x);
    if (std::strtod(buf, nullptr) == x) break;
  }
  // buf is "d.ddde±XX": the mantissa's digits and the exponent of its first digit.
  const char* e = buf;
  while (*e != 'e') e++;
  digits.clear();
  for (const char* p = buf; p < e; p++) {
    if (*p != '.') digits += *p;
  }
  while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
  exp10 = std::atoi(e + 1) + 1;
}

}  // namespace

std::string to_string(double x) {
  if (x != x) return "NaN";
  if (x == 0) return "0";
  if (std::isinf(x)) return x > 0 ? "Infinity" : "-Infinity";
  std::string out = x < 0 ? "-" : "";
  std::string d;
  int n = 0;  // x = 0.d × 10^n
  shortest(std::abs(x), d, n);
  const int k = static_cast<int>(d.size());
  if (k <= n && n <= 21) {
    out += d + std::string(static_cast<size_t>(n - k), '0');
  } else if (0 < n && n <= 21) {
    out += d.substr(0, static_cast<size_t>(n)) + "." + d.substr(static_cast<size_t>(n));
  } else if (-6 < n && n <= 0) {
    out += "0." + std::string(static_cast<size_t>(-n), '0') + d;
  } else {
    out += d.substr(0, 1);
    if (k > 1) out += "." + d.substr(1);
    out += (n - 1 >= 0 ? "e+" : "e-") + std::to_string(std::abs(n - 1));
  }
  return out;
}

std::string to_fixed(double x, int digits) {
  if (x != x) return "NaN";
  if (std::abs(x) >= 1e21) return to_string(x);
  const bool negative = x < 0;
  // Every double has a finite decimal expansion of at most 1074 places: print it whole, then
  // round it at `digits` places half away from zero, which is what toFixed's "the larger n on
  // a tie" is on the magnitude.
  static thread_local char buf[1500];
  std::snprintf(buf, sizeof buf, "%.1100f", std::abs(x));
  std::string s(buf);
  const size_t dot = s.find('.');
  std::string whole = s.substr(0, dot);
  std::string frac = s.substr(dot + 1);
  const bool up = frac[static_cast<size_t>(digits)] >= '5';
  std::string kept = whole + frac.substr(0, static_cast<size_t>(digits));
  if (up) {
    size_t i = kept.size();
    while (i > 0) {
      i--;
      if (kept[i] == '9') {
        kept[i] = '0';
      } else {
        kept[i]++;
        break;
      }
      if (i == 0) kept.insert(kept.begin(), '1');
    }
  }
  const size_t int_len = kept.size() - static_cast<size_t>(digits);
  std::string out = negative ? "-" : "";
  out += kept.substr(0, int_len);
  if (digits > 0) out += "." + kept.substr(int_len);
  return out;
}

}  // namespace ge::core::js
