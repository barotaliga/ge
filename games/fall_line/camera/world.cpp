// The mountain's three answers to the camera.
//
// ge:summary The mountain's three answers to the camera: ground, trunks, posts
// ge:keywords fall-line camera world ground trunks posts
// ge:skill camera
// ge:test tests/fall_line_camera_test.cpp
#include "fall_line/camera/world.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "skier/trees.hpp"

namespace fall_line::camera {

namespace cam = ge::camera;
namespace libm = ge::core::libm;
using ge::mountain::mapgen::Level;

namespace {

double ground(const void* ctx, double x, double z) {
  return static_cast<const ge::skier::World*>(ctx)->level->ground_at(x, z);
}

int trunks(const void* ctx, double x, double z, double reach, cam::Obstacle* out, int most) {
  const auto* w = static_cast<const ge::skier::World*>(ctx);
  if (!w->trees) return 0;
  const Level& level = *w->level;
  int n = 0;
  ge::skier::for_each_tree_near(*w->trees, x, z, [&](int32_t i) {
    if (n >= most) return;
    const auto& t = level.trees[static_cast<size_t>(i)];
    const double dx = t.x - x, dz = t.z - z;
    const double r = reach + t.radius;
    if (dx * dx + dz * dz > r * r) return;
    out[n++] = cam::Obstacle{t.x, t.z, t.radius, t.y, t.y + t.height, static_cast<uint32_t>(i)};
  });
  return n;
}

// The share of the segment a..b in plan before it comes within `r` of (cx, cz), and whether the
// height there lies between y0 and y1: 1 when it never does.
double post_share(const cam::Vec3& a, const cam::Vec3& b, double cx, double cz, double r, double y0,
                  double y1) {
  const double dx = b.x - a.x, dz = b.z - a.z;
  const double fx = a.x - cx, fz = a.z - cz;
  const double qa = dx * dx + dz * dz;
  const double qc = fx * fx + fz * fz - r * r;
  if (qc <= 0) return 1;  // the line starts inside it (the skier at a post): never pull onto him
  if (qa < 1e-12) return 1;
  const double qb = 2 * (fx * dx + fz * dz);
  const double disc = qb * qb - 4 * qa * qc;
  if (disc < 0) return 1;
  const double t = (-qb - std::sqrt(disc)) / (2 * qa);
  if (t < 0 || t > 1) return 1;
  const double y = a.y + (b.y - a.y) * t;
  return y >= y0 && y <= y1 ? t : 1;
}

double posts(const void* ctx, const cam::Vec3& from, const cam::Vec3& to) {
  const Level& level = *static_cast<const ge::skier::World*>(ctx)->level;
  const double ax = to.x - from.x, az = to.z - from.z;
  const double reach = std::sqrt(ax * ax + az * az) + kPostPad + 1;
  double share = 1;
  for (const auto& c : level.checkpoints) {
    const double mx = c.x - from.x, mz = c.z - from.z;
    if (mx * mx + mz * mz > (reach + c.width) * (reach + c.width)) continue;
    // Across the gate: heading 0 = +z, clockwise from above, so right is (cos h, -sin h).
    const double rx = libm::cos(c.heading) * c.width / 2, rz = -libm::sin(c.heading) * c.width / 2;
    for (const double side : {-1.0, 1.0}) {
      const double s = post_share(from, to, c.x + side * rx, c.z + side * rz, kPostPad, c.y - 1,
                                  c.y + kPostHeight + kPostPad);
      share = s < share ? s : share;
    }
  }
  return share;
}

}  // namespace

cam::World camera_world(const ge::skier::World& skier_world) {
  cam::World w;
  w.ctx = &skier_world;
  w.ground = ground;
  w.obstacles = trunks;
  w.clear = posts;
  return w;
}

}  // namespace fall_line::camera
