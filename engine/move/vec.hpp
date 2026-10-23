// THE VECTOR ARITHMETIC character movement reads: `core::Vec3` added, scaled, dotted and crossed,
// inline, every operation a correctly rounded IEEE one (no transcendental, no fused multiply-add
// under the build's flags), so a step is the same bits everywhere.
//
// ge:summary Vector arithmetic for character movement: add, scale, dot, cross, length, flat
// ge:keywords move movement vector vec3 add sub scale dot cross length flat horizontal
// ge:skill move determinism
// ge:test tests/move_step_test.cpp
#pragma once

#include <cmath>

#include "core/quat.hpp"

namespace ge::move {

using core::Vec3;

inline Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 scale(const Vec3& a, double k) { return {a.x * k, a.y * k, a.z * k}; }
// a + b * k, as two roundings (the product, then the sum).
inline Vec3 add_scaled(const Vec3& a, const Vec3& b, double k) {
  return {a.x + b.x * k, a.y + b.y * k, a.z + b.z * k};
}
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
// The plan's part (y dropped) and its length: the speed a speedometer reads.
inline Vec3 flat(const Vec3& a) { return {a.x, 0.0, a.z}; }
inline double flat_length(const Vec3& a) { return std::sqrt(a.x * a.x + a.z * a.z); }
// `a` at unit length, or zero when it has none.
inline Vec3 unit(const Vec3& a) {
  const double n = length(a);
  return n > 0.0 ? scale(a, 1.0 / n) : Vec3{0.0, 0.0, 0.0};
}

}  // namespace ge::move
