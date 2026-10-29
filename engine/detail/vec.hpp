// THE SMALL VECTOR ARITHMETIC the detail passes share: `core::Vec3` added, scaled, dotted, crossed
// and normalised, inline. Exact IEEE operations only (sqrt is correctly rounded), so every bevel,
// decal and prop placed from them keeps its bits on every compiler.
//
// ge:summary Inline Vec3 arithmetic for the detail passes: add, sub, dot, cross, normalise
// ge:keywords detail vec3 vector dot cross normalize length add sub scale ramp smoothstep
// ge:skill detail
// ge:test tests/detail_solid_test.cpp
#pragma once

#include <cmath>

#include "core/quat.hpp"

namespace ge::detail {

using Vec3 = core::Vec3;

inline Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 mul(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
// a + b * s, the step every walk along a direction takes.
inline Vec3 mad(const Vec3& a, const Vec3& b, double s) {
  return {a.x + b.x * s, a.y + b.y * s, a.z + b.z * s};
}
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
// `a` at unit length; a zero vector stays zero.
inline Vec3 normalized(const Vec3& a) {
  const double n = length(a);
  return n > 0 ? mul(a, 1 / n) : a;
}
// A Hermite ease from 0 at `a` to 1 at `b`, clamped, for `a` above `b` as well as below.
inline double ramp(double a, double b, double v) {
  double t = (v - a) / (b - a);
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return t * t * (3 - 2 * t);
}

// Some unit vector at right angles to the unit `n`, chosen the same way every time.
inline Vec3 any_perpendicular(const Vec3& n) {
  const Vec3 axis = (n.y > 0.9 || n.y < -0.9) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
  return normalized(cross(axis, n));
}

}  // namespace ge::detail
