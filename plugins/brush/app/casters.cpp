// The moving casters' lights and capsules (casters.hpp).
//
// ge:summary The moving casters' lights ranked for their cubes, and their capsules
// ge:keywords bsp brush model-shadows caster_lights shadow_capsules
// ge:skill brush render
// ge:test tests/brush_bsp_casters_test.cpp
#include "brush/app/casters.hpp"

#include <algorithm>
#include <utility>

namespace ge::brush::app {

namespace {

// The square of the distance from `p` to the middle of `b`.
double middle_d2(const CasterBox& b, const double p[3]) {
  double d2 = 0;
  for (int k = 0; k < 3; ++k) {
    const double m = (double(b.lo[k]) + b.hi[k]) / 2 - p[k];
    d2 += m * m;
  }
  return d2;
}

}  // namespace

std::vector<size_t> caster_lights(std::span<const GpuLight> lights,
                                  std::span<const size_t> shadowed,
                                  std::span<const CasterBox> boxes, const double eye[3]) {
  std::vector<std::pair<double, size_t>> lit_by;
  for (const size_t c : shadowed) {
    const GpuLight& g = lights[c];
    if (g.info[0] < 0) continue;  // no cube of the world this frame: unshadowed
    double best = -1;
    for (const CasterBox& b : boxes) {
      double d2 = 0;
      for (int k = 0; k < 3; ++k) {
        const double e = std::max({double(b.lo[k]) - g.pos[k], 0.0, g.pos[k] - double(b.hi[k])});
        d2 += e * e;
      }
      if (d2 >= double(g.pos[3]) * g.pos[3]) continue;
      const double seen = 1 / (1 + middle_d2(b, eye) / (kCasterSeenM * kCasterSeenM));
      best = std::max(
          best, (double(g.colour[0]) + g.colour[1] + g.colour[2]) / (d2 + g.info[1] + 0.25) * seen);
    }
    if (best > 0) lit_by.push_back({-best, c});
  }
  std::sort(lit_by.begin(), lit_by.end());  // ties: the lower index first
  std::vector<size_t> out;
  for (const auto& [rank, c] : lit_by) {
    if (out.size() >= size_t(kCasterLights)) break;
    out.push_back(c);
  }
  return out;
}

std::vector<Capsule> shadow_capsules(std::span<const CasterBox> boxes, const double eye[3]) {
  std::vector<std::pair<double, size_t>> near;
  for (size_t i = 0; i < boxes.size(); ++i) near.push_back({middle_d2(boxes[i], eye), i});
  std::sort(near.begin(), near.end());
  std::vector<Capsule> out;
  for (size_t c = 0; c < near.size() && c < size_t(kShadowCapsules); ++c) {
    const CasterBox& b = boxes[near[c].second];
    Capsule cap;
    cap.r = 0.25 * ((double(b.hi[0]) - b.lo[0]) + (double(b.hi[2]) - b.lo[2])) * 0.5;
    const double x = (double(b.lo[0]) + b.hi[0]) / 2, z = (double(b.lo[2]) + b.hi[2]) / 2;
    const double bottom = b.lo[1] + cap.r, top = std::max(double(b.hi[1]) - cap.r, bottom);
    cap.a[0] = cap.b[0] = x;
    cap.a[2] = cap.b[2] = z;
    cap.a[1] = bottom;
    cap.b[1] = top;
    out.push_back(cap);
  }
  return out;
}

}  // namespace ge::brush::app
