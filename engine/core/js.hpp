// JAVASCRIPT NUMBER SEMANTICS. The JS operators and Math functions game5's engine uses whose
// C++ look-alikes give different bits or undefined behaviour (docs/porting.md "JavaScript
// semantics that bite"). A port spells each of these through js::, never through the
// look-alike, unless the inputs provably avoid the difference (and a comment says why).
// Transcendentals live in core/libm/libm.hpp; Math.hypot in core/math.hpp.
// Parity: tests/core_js_test.cpp against tests/golden/core_js.txt (tools/parity/golden-js.mjs).
//
//   JS                      here                 not this
//   Math.round(x)           js::round(x)         std::round (half away from 0), floor(x + 0.5)
//   x | 0                   js::to_int32(x)      static_cast<int32_t> (UB out of range)
//   x >>> 0                 js::to_uint32(x)     static_cast<uint32_t> (UB out of range)
//   Math.imul(a, b)         js::imul(a, b)       a * b on int32_t (signed overflow is UB)
//   Math.max / Math.min     js::max / js::min    std::max/min (NaN and -0 wrong)
//   Math.sign(x)            js::sign(x)          a hand-rolled ternary (-0 and NaN wrong)
//   a % b                   js::mod(a, b)        std::remainder (rounds the quotient)
//
// ge:summary JavaScript number semantics: Math.round, |0, >>>0, Math.imul, max/min, sign, %
// ge:keywords js javascript semantics round to-int32 to-uint32 imul max min sign mod modulo
// ge:keywords negative-zero nan
// ge:skill determinism
// ge:test tests/core_js_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ge::core::js {

// Math.round: the nearest integer, halves toward +inf (round(-2.5) == -2), keeping -0
// (round(-0.4) == -0). V8's recipe: ceil, then step back when ceil overshot by more than 0.5.
// Exact for every double, including 0.49999999999999994 where floor(x + 0.5) gives 1.
inline double round(double x) {
  const double c = std::ceil(x);
  return c - 0.5 > x ? c - 1.0 : c;
}

// ToInt32 (`x | 0`): NaN and +-inf become 0, the rest truncates toward zero and wraps
// modulo 2^32 into [-2^31, 2^31).
inline int32_t to_int32(double x) {
  if (x >= -2147483648.0 && x < 2147483648.0) return static_cast<int32_t>(x);  // fast path
  if (!std::isfinite(x)) return 0;
  double m = std::fmod(std::trunc(x), 4294967296.0);  // exact
  if (m < 0) m += 4294967296.0;                       // exact: |m| < 2^32
  return static_cast<int32_t>(static_cast<uint32_t>(m));
}

// ToUint32 (`x >>> 0`): the same 32 bits as to_int32, read unsigned.
inline uint32_t to_uint32(double x) { return static_cast<uint32_t>(to_int32(x)); }

// Math.imul on values already in int32/uint32 (JS code passes `|0` / `>>>0` words): the low
// 32 bits of the product, as int32. For a double argument, convert with to_int32 first.
inline int32_t imul(uint32_t a, uint32_t b) { return static_cast<int32_t>(a * b); }

// Math.max(a, b): NaN if either is NaN; +0 is greater than -0.
inline double max(double a, double b) {
  if (a != a || b != b) return std::numeric_limits<double>::quiet_NaN();
  if (a == b) return std::signbit(a) ? b : a;
  return a > b ? a : b;
}

// Math.min(a, b): NaN if either is NaN; -0 is less than +0.
inline double min(double a, double b) {
  if (a != a || b != b) return std::numeric_limits<double>::quiet_NaN();
  if (a == b) return std::signbit(a) ? a : b;
  return a < b ? a : b;
}

// Math.max / Math.min of three or more: folded left to right, as JS reads its arguments.
template <class... Rest>
inline double max(double a, double b, double c, Rest... rest) {
  return max(max(a, b), c, rest...);
}
template <class... Rest>
inline double min(double a, double b, double c, Rest... rest) {
  return min(min(a, b), c, rest...);
}

// Math.sign: -1, +1, or the argument itself when it is +-0 or NaN.
inline double sign(double x) { return x > 0 ? 1.0 : x < 0 ? -1.0 : x; }

// a % b on numbers: the exact remainder with the sign of a (fmod), NaN for b == 0.
inline double mod(double a, double b) { return std::fmod(a, b); }

}  // namespace ge::core::js
