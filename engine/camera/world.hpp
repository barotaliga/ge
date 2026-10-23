// WHAT A CAMERA ASKS OF THE WORLD: three questions the game answers, as plain function pointers
// over its own data (no collision system is assumed, and nothing is allocated):
//
//   ground(x, z)            the surface height under a point, m: a boom's clearance and incline
//   clear(from, to)         the share (0..1) of the segment open before it meets something solid:
//                           a boom's pull-in, a planted lens's line of sight
//   obstacles(x, z, reach)  the point obstacles (trunks, posts) within `reach` in plan: a boom's
//                           push-off
//
// A thing is answered by ONE of `clear` and `obstacles`, never both: a boom pulls its arm in at
// once for what `clear` reports (a wall, the ground, a post it must never stand inside) and is
// pushed sideways off what `obstacles` reports (the trunks of a wood it passes through), and a
// thing in both would be pulled onto while being pushed off.
//
// A query left null answers "nothing there": no ground (minus infinity), the line open, no
// obstacles. A shooter answers `clear` with a trace through its map's hull; a skiing game with its
// trees, posts and arches.
//
// ge:summary The world's queries a camera asks: ground height, line clear, obstacles near
// ge:keywords camera world query ground terrain clearance line-of-sight occlusion clear trace
// ge:keywords obstacles trunks posts collision callback ground_at clear_share obstacles_near
// ge:skill camera
// ge:test tests/camera_boom_test.cpp
#pragma once

#include <cstdint>
#include <limits>

#include "core/quat.hpp"

namespace ge::camera {

using core::Vec3;

struct Obstacle {
  double x = 0, z = 0;    // world metres, its centre in plan
  double radius = 0;      // m
  double y0 = 0, y1 = 0;  // the heights it runs between, m
  uint32_t id = 0;        // stable across frames (the push-off remembers its side)
};

struct World {
  const void* ctx = nullptr;
  double (*ground)(const void* ctx, double x, double z) = nullptr;
  double (*clear)(const void* ctx, const Vec3& from, const Vec3& to) = nullptr;
  int (*obstacles)(const void* ctx, double x, double z, double reach, Obstacle* out,
                   int most) = nullptr;
};

inline double ground_at(const World& w, double x, double z) {
  return w.ground ? w.ground(w.ctx, x, z) : -std::numeric_limits<double>::infinity();
}
inline double clear_share(const World& w, const Vec3& from, const Vec3& to) {
  return w.clear ? w.clear(w.ctx, from, to) : 1.0;
}
inline int obstacles_near(const World& w, double x, double z, double reach, Obstacle* out,
                          int most) {
  return w.obstacles ? w.obstacles(w.ctx, x, z, reach, out, most) : 0;
}

}  // namespace ge::camera
