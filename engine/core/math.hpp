// SMALL DETERMINISTIC MATH. Port of the libm-free half of oss-game-framework
// `src/core/math.ts`. Every function here uses only IEEE 754 correctly
// rounded operations (+ - * / sqrt, fmod, compares), so it returns the same
// bits on every compiler that keeps `-ffp-contract=off` (see AGENTS.md, "The
// determinism contract"). `decay` and `blockOffsets` are not ported: game5
// calls neither. Parity: tests/core_math_test.cpp.
//
// World convention: the plan is x/z, y is up; heading 0 points down +z and
// grows clockwise seen from above.
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include "core/js.hpp"

namespace ge::core {

inline constexpr double kPi = 3.141592653589793;
inline constexpr double kTau = kPi * 2.0;

inline double clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

// Hermite ease from 0 at a to 1 at b, clamped either side.
inline double smoothstep(double a, double b, double v) {
  const double t = v <= a ? 0.0 : v >= b ? 1.0 : (v - a) / (b - a);
  return t * t * (3.0 - 2.0 * t);
}

// Signed shortest angular difference b - a, in (-pi, pi]. JS `%` is fmod.
inline double angle_diff(double a, double b) {
  double d = js::mod(b - a, kTau);
  if (d > kPi) d -= kTau;
  if (d <= -kPi) d += kTau;
  return d;
}

// Move v toward target by at most max_delta.
inline double approach(double v, double target, double max_delta) {
  const double d = target - v;
  if (std::fabs(d) <= max_delta) return target;
  return v + js::sign(d) * max_delta;
}

// JS `Math.hypot(a, b)` bit for bit (V8's scale-by-max recipe). A bare
// sqrt(a*a + b*b) differs in the last bit about a third of the time.
inline double hypot(double a, double b) {
  const double x = std::fabs(a);
  const double y = std::fabs(b);
  constexpr double inf = std::numeric_limits<double>::infinity();
  if (x == inf || y == inf) return inf;
  if (x != x || y != y) return std::numeric_limits<double>::quiet_NaN();
  const double m = x > y ? x : y;
  if (m == 0.0) return 0.0;
  const double p = x / m;
  const double q = y / m;
  return std::sqrt(p * p + q * q) * m;
}

// JS `Math.hypot(a, b, c)` bit for bit: Kahan compensation carried into c.
inline double hypot3(double a, double b, double c) {
  const double x = std::fabs(a);
  const double y = std::fabs(b);
  const double z = std::fabs(c);
  constexpr double inf = std::numeric_limits<double>::infinity();
  if (x == inf || y == inf || z == inf) return inf;
  if (x != x || y != y || z != z) return std::numeric_limits<double>::quiet_NaN();
  const double m = x > y ? (x > z ? x : z) : y > z ? y : z;
  if (m == 0.0) return 0.0;
  const double p = x / m;
  const double q = y / m;
  const double r = z / m;
  const double pp = p * p;
  const double qq = q * q;
  const double two = pp + qq;
  const double carried = two - pp - qq;
  return std::sqrt(two + (r * r - carried)) * m;
}

// JS `Math.hypot(a, b, c, d)` bit for bit — a quaternion's length.
inline double hypot4(double a, double b, double c, double d) {
  const double x = std::fabs(a);
  const double y = std::fabs(b);
  const double z = std::fabs(c);
  const double w = std::fabs(d);
  constexpr double inf = std::numeric_limits<double>::infinity();
  if (x == inf || y == inf || z == inf || w == inf) return inf;
  if (x != x || y != y || z != z || w != w) return std::numeric_limits<double>::quiet_NaN();
  double m = x > y ? x : y;
  if (z > m) m = z;
  if (w > m) m = w;
  if (m == 0.0) return 0.0;
  const double p = x / m;
  const double q = y / m;
  const double r = z / m;
  const double t = w / m;
  const double pp = p * p;
  const double qq = q * q;
  const double two = pp + qq;
  double carried = two - pp - qq;
  const double third = r * r - carried;
  const double three = two + third;
  carried = three - two - third;
  return std::sqrt(three + (t * t - carried)) * m;
}

inline double dist2(double ax, double az, double bx, double bz) {
  const double dx = bx - ax;
  const double dz = bz - az;
  return dx * dx + dz * dz;
}

// One integer key for a spatial-hash cell; injective while |iz| < 4096.
// JS computes it in double; every key in range is an exact integer there.
inline int64_t cell_key(int32_t ix, int32_t iz) { return static_cast<int64_t>(ix) * 8192 + iz; }

}  // namespace ge::core
