// What Shooter's Edge's navigation labs (`se-nav`, `se-pilot`) draw a map from above with: the
// view to scale (a pixel a unit, x east, y north up the picture, a margin round it for a legend's
// row) and the floor shaded by height, traced down the map's own point hull per pixel (solid at
// the top: a wall, dark). Header-only, beside the labs (a lab's helper never goes into a library:
// the labs skill).
//
// ge:summary The nav labs' top view: a map's floor from above to scale, shaded by height
// ge:keywords lab nav top-view floor height-shade point-hull view margin clip-top roof cut-away
// ge:keywords floor_below see-under-the-roof
// ge:skill shooter labs
// ge:test tests/shooter_nav_test.cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

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

// THE MAP CLIPPED FROM ABOVE: the floor under (x, y) seen from `top` with everything above `top`
// cut away, the roof included: from `top` down through any solid (a roof, a ceiling between two
// levels) to the first open space, then down to the floor. `found` false when the column is solid
// all the way to `bottom` (a wall); `open` when nothing is under the open space (a void).
struct ClippedFloor {
  bool found = false, open = false;
  float z = 0;
};
inline ClippedFloor floor_below(const sh::Hull& hull, float x, float y, float top, float bottom) {
  constexpr float kThrough = 4;  // the solid stepped through this finely, units
  ClippedFloor out;
  float from = top;
  while (from > bottom && sh::point_contents(hull, {x, y, from}) == sh::kContentsSolid)
    from -= kThrough;
  if (from <= bottom) return out;
  const sh::HullTrace tr = sh::trace_hull(hull, {x, y, from}, {x, y, bottom});
  out.found = !tr.startsolid;
  out.open = tr.fraction >= 1;
  out.z = tr.endpos.z;
  return out;
}

// `clip_top` (a height, `--clip-top` on the labs that draw from above): the map above it cut away
// and every roof under it stepped through (`floor_below`), so a level under another, or a room
// under its roof, is seen; NaN (the default): traced from the view's top as always, a pixel whose
// trace starts in solid drawn dark.
inline gl::Picture floor_picture(const sh::MoveWorld& world, const TopView& v,
                                 float clip_top = std::numeric_limits<float>::quiet_NaN()) {
  const int w = static_cast<int>(v.hi.x - v.lo.x) + 2 * kTopMargin;
  const int h = static_cast<int>(v.hi.y - v.lo.y) + 2 * kTopMargin;
  gl::Picture p = gl::create_picture(w, h, {245, 245, 240, 255});
  const sh::Hull& hull = world.solids[0].point_hull;
  const float span = v.hi.z - v.lo.z;
  for (int py = kTopMargin; py < h - kTopMargin; py++)
    for (int px = kTopMargin; px < w - kTopMargin; px++) {
      const float x = v.lo.x + static_cast<float>(px - kTopMargin) + 0.5f;
      const float y = v.hi.y - static_cast<float>(py - kTopMargin) - 0.5f;
      float z = 0;
      if (std::isnan(clip_top)) {
        const sh::HullTrace tr = sh::trace_hull(hull, {x, y, v.hi.z - 1}, {x, y, v.lo.z});
        if (tr.startsolid) {
          gl::set_pixel(p, px, py, {70, 74, 72, 255});
          continue;
        }
        z = tr.endpos.z;
      } else {
        const ClippedFloor f = floor_below(hull, x, y, std::min(clip_top, v.hi.z - 1), v.lo.z);
        if (!f.found) {
          gl::set_pixel(p, px, py, {70, 74, 72, 255});
          continue;
        }
        z = f.z;
      }
      const float t = (z - v.lo.z) / span;  // 0 low .. 1 high
      const auto g = static_cast<uint8_t>(150 + 95 * t);
      gl::set_pixel(p, px, py,
                    {static_cast<uint8_t>(g - 20), g, static_cast<uint8_t>(g - 35), 255});
    }
  return p;
}

}  // namespace shooters_edge::labs
