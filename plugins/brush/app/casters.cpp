// The moving casters' lights and capsules (casters.hpp).
//
// ge:summary The moving casters' lights ranked for their cubes, and their capsules
// ge:keywords bsp brush model-shadows caster_lights shadow_capsules caster_cones
// ge:skill brush render
// ge:test tests/brush_bsp_casters_test.cpp
#include "brush/app/casters.hpp"

#include <algorithm>
#include <cmath>
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

namespace {

constexpr double kPi = 3.14159265358979323846;

// A cone of ways from a light: its unit axis and half-angle, radians (pi or more: every way).
struct Cone {
  double axis[3];
  double half;
};

// The narrowest cone holding both `a` and `b`.
Cone merged(const Cone& a, const Cone& b) {
  const double c =
      std::clamp(a.axis[0] * b.axis[0] + a.axis[1] * b.axis[1] + a.axis[2] * b.axis[2], -1.0, 1.0);
  const double between = std::acos(c);
  if (between + b.half <= a.half) return a;
  if (between + a.half <= b.half) return b;
  Cone out{};
  out.half = (between + a.half + b.half) / 2;
  if (out.half >= kPi || between < 1e-9) {
    out = a;
    out.half = std::max({a.half, b.half, out.half});
    return out;
  }
  // The axis turned from a's toward b's by (half - a.half) along the great circle between them.
  const double turn = out.half - a.half;
  double perp[3];
  for (int k = 0; k < 3; ++k) perp[k] = b.axis[k] - c * a.axis[k];
  const double pl = std::sqrt(perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2]);
  for (int k = 0; k < 3; ++k)
    out.axis[k] = a.axis[k] * std::cos(turn) + perp[k] / pl * std::sin(turn);
  return out;
}

}  // namespace

bool box_in_face(const CasterBox& b, const float pos[3], float reach, int face) {
  double near2 = 0, lo[3], hi[3];
  for (int k = 0; k < 3; ++k) {
    lo[k] = double(b.lo[k]) - pos[k];
    hi[k] = double(b.hi[k]) - pos[k];
    const double e = std::max({lo[k], 0.0, -hi[k]});
    near2 += e * e;
  }
  if (near2 >= double(reach) * reach) return false;
  const int i = face / 2;
  // The furthest the box goes along the face's way, and the least of each other axis's size.
  const double along = face % 2 == 0 ? hi[i] : -lo[i];
  if (along < 0) return false;
  for (int k = 0; k < 3; ++k) {
    if (k == i) continue;
    const double least = std::max({lo[k], 0.0, -hi[k]});
    if (least > along) return false;
  }
  return true;
}

CasterCones caster_cones(const GpuLight& light, std::span<const CasterBox> boxes, double widen) {
  CasterCones out{};
  out.nearest = light.pos[3];
  std::vector<Cone> cones;
  bool everywhere = false;
  for (const CasterBox& b : boxes) {
    double to[3], r2 = 0, d2 = 0;
    for (int k = 0; k < 3; ++k) {
      to[k] = (double(b.lo[k]) + b.hi[k]) / 2 - light.pos[k];
      const double h = (double(b.hi[k]) - b.lo[k]) / 2;
      r2 += h * h;
      d2 += to[k] * to[k];
    }
    const double r = std::sqrt(r2), d = std::sqrt(d2);
    if (d - r >= double(light.pos[3])) continue;  // wholly past the reach: hides nothing in it
    out.nearest = std::min(out.nearest, float(std::max(d - r, 0.0)));
    if (d <= r * 1.0001 + 1e-4) {
      everywhere = true;
      continue;
    }
    Cone c{{to[0] / d, to[1] / d, to[2] / d}, std::asin(r / d)};
    cones.push_back(c);
  }
  // Too many: merge the pair whose union is narrowest, until kCasterCones are left.
  while (cones.size() > size_t(kCasterCones)) {
    size_t bi = 0, bj = 1;
    Cone best{};
    best.half = 1e30;
    for (size_t i = 0; i < cones.size(); ++i)
      for (size_t j = i + 1; j < cones.size(); ++j) {
        const Cone m = merged(cones[i], cones[j]);
        if (m.half < best.half) {
          best = m;
          bi = i;
          bj = j;
        }
      }
    cones[bi] = best;
    cones.erase(cones.begin() + std::ptrdiff_t(bj));
  }
  for (int i = 0; i < kCasterCones; ++i) {
    float* w = out.cone[i];
    w[0] = w[1] = w[2] = 0;
    w[3] = 2;  // no way inside
    if (size_t(i) < cones.size()) {
      const Cone& c = cones[size_t(i)];
      for (int k = 0; k < 3; ++k) w[k] = float(c.axis[k]);
      // A little short of the cosine, so the float's rounding never leaves a way out.
      const double half = c.half + widen;
      w[3] = half >= kPi ? -2.0f : float(std::cos(half) - 1e-5);
    }
  }
  if (everywhere) out.cone[0][3] = -2;
  return out;
}

}  // namespace ge::brush::app
