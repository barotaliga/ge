// What Shooter's Edge's navigation labs (`se-nav`, `se-pilot`) draw a map from above with: the
// view to scale (a pixel a unit, x east, y north up the picture, a margin round it for a legend's
// row) and the floor shaded by height, traced down the map's own point hull per pixel (solid at
// the top: a wall, dark). Header-only, beside the labs (a lab's helper never goes into a library:
// the labs skill).
//
// ge:summary The nav labs' top view: a map's floor from above to scale, shaded by height
// ge:keywords lab nav top-view floor height-shade point-hull view margin
// ge:skill shooter labs
// ge:test tests/shooter_nav_test.cpp
#pragma once

#include <cstdint>

#include "lab/raster.hpp"
#include "shooter/hull.hpp"
#include "shooter/world.hpp"

namespace shooters_edge::labs {

namespace gl = ge::lab;
namespace sh = ge::shooter;

inline constexpr int kTopMargin = 24;  // pixels round the map, for the legend's row

// The picture's pixel for a map point, one pixel a unit.
struct TopView {
  sh::Vec3f lo, hi;
  double px(double x) const { return kTopMargin + (x - lo.x); }
  double py(double y) const { return kTopMargin + (hi.y - y); }
};

inline gl::Picture floor_picture(const sh::MoveWorld& world, const TopView& v) {
  const int w = static_cast<int>(v.hi.x - v.lo.x) + 2 * kTopMargin;
  const int h = static_cast<int>(v.hi.y - v.lo.y) + 2 * kTopMargin;
  gl::Picture p = gl::create_picture(w, h, {245, 245, 240, 255});
  const sh::Hull& hull = world.solids[0].point_hull;
  const float span = v.hi.z - v.lo.z;
  for (int py = kTopMargin; py < h - kTopMargin; py++)
    for (int px = kTopMargin; px < w - kTopMargin; px++) {
      const float x = v.lo.x + static_cast<float>(px - kTopMargin) + 0.5f;
      const float y = v.hi.y - static_cast<float>(py - kTopMargin) - 0.5f;
      const sh::HullTrace tr = sh::trace_hull(hull, {x, y, v.hi.z - 1}, {x, y, v.lo.z});
      if (tr.startsolid) {
        gl::set_pixel(p, px, py, {70, 74, 72, 255});
        continue;
      }
      const float t = (tr.endpos.z - v.lo.z) / span;  // 0 low .. 1 high
      const auto g = static_cast<uint8_t>(150 + 95 * t);
      gl::set_pixel(p, px, py,
                    {static_cast<uint8_t>(g - 20), g, static_cast<uint8_t>(g - 35), 255});
    }
  return p;
}

}  // namespace shooters_edge::labs
