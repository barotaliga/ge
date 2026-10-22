// THE SIMULATION'S NUMBER SEMANTICS. The rounding, integer conversion and comparison rules
// the simulation is defined in, where the C++ look-alike gives different bits or undefined
// behaviour. Simulation code spells each of these through num::, never through the
// look-alike, unless the inputs provably avoid the difference (and a comment says why).
// Transcendentals live in core/libm/libm.hpp; hypot in core/math.hpp.
// Bits: tests/core_num_test.cpp against tests/golden/core_num.txt.
//
//   here                 the rule                            not this
//   num::round(x)        nearest, halves toward +inf         std::round, floor(x + 0.5)
//   num::to_int32(x)     truncate, wrap mod 2^32             static_cast (UB out of range)
//   num::to_uint32(x)    the same word, unsigned             static_cast (UB out of range)
//   num::imul(a, b)      low 32 bits of the product          int32_t * (overflow is UB)
//   num::max / min       NaN wins; -0 < +0                   std::max/min (NaN, -0 wrong)
//   num::sign(x)         -1, +1, or +-0 / NaN itself         a ternary (-0 and NaN wrong)
//   num::mod(a, b)       exact, with the sign of a (fmod)    std::remainder
//
// ge:summary The number semantics: round, to_int32, to_uint32, imul, max/min, sign, mod
// ge:keywords num number semantics round to-int32 to-uint32 imul max min sign mod modulo
// ge:keywords negative-zero nan
// ge:skill determinism
// ge:test tests/core_num_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ge::core::num {

// The nearest integer, halves toward +inf (round(-2.5) == -2), keeping -0
// (round(-0.4) == -0). The recipe: ceil, then step back when ceil overshot by more than 0.5.
// Exact for every double, including 0.49999999999999994 where floor(x + 0.5) gives 1.
inline double round(double x) {
  const double c = std::ceil(x);
  return c - 0.5 > x ? c - 1.0 : c;
}

// To int32: NaN and +-inf become 0, the rest truncates toward zero and wraps
// modulo 2^32 into [-2^31, 2^31).
inline int32_t to_int32(double x) {
  if (x >= -2147483648.0 && x < 2147483648.0) return static_cast<int32_t>(x);  // fast path
  // Within int64's range the truncation is exact as an int64, and its low 32 bits are the
  // wrap: the same word as the remainder below, without the library call a hash pays for.
  if (x >= -9223372036854775808.0 && x < 9223372036854775808.0) {
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(x)));
  }
  if (!std::isfinite(x)) return 0;
  double m = std::fmod(std::trunc(x), 4294967296.0);  // exact
  if (m < 0) m += 4294967296.0;                       // exact: |m| < 2^32
  return static_cast<int32_t>(static_cast<uint32_t>(m));
}

// To uint32: the same 32 bits as to_int32, read unsigned.
inline uint32_t to_uint32(double x) { return static_cast<uint32_t>(to_int32(x)); }

// The int32 multiply of two 32-bit words: the low 32 bits of the product, as int32. For a
// double argument, convert with to_int32 first.
inline int32_t imul(uint32_t a, uint32_t b) { return static_cast<int32_t>(a * b); }

// max(a, b): NaN if either is NaN; +0 is greater than -0.
inline double max(double a, double b) {
  if (a != a || b != b) return std::numeric_limits<double>::quiet_NaN();
  if (a == b) return std::signbit(a) ? b : a;
  return a > b ? a : b;
}

// min(a, b): NaN if either is NaN; -0 is less than +0.
inline double min(double a, double b) {
  if (a != a || b != b) return std::numeric_limits<double>::quiet_NaN();
  if (a == b) return std::signbit(a) ? a : b;
  return a < b ? a : b;
}

// max / min of three or more: folded left to right.
template <class... Rest>
inline double max(double a, double b, double c, Rest... rest) {
  return max(max(a, b), c, rest...);
}
template <class... Rest>
inline double min(double a, double b, double c, Rest... rest) {
  return min(min(a, b), c, rest...);
}

// sign: -1, +1, or the argument itself when it is +-0 or NaN.
inline double sign(double x) { return x > 0 ? 1.0 : x < 0 ? -1.0 : x; }

// The remainder of a / b: the exact remainder with the sign of a (fmod), NaN for b == 0.
// A dividend already smaller than the divisor is its own remainder (fmod returns it as it is,
// -0 included), so that case, the common one for an angle, skips the library call.
inline double mod(double a, double b) {
  if (std::fabs(a) < std::fabs(b)) return a;
  return std::fmod(a, b);
}

}  // namespace ge::core::num
