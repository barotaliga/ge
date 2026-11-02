// The ice on a lake as drawn: the sheet's cells inside the shore and the two parts' surfaces.
//
// ge:summary A frozen lake as drawn: the sheet's grid and the look's blend by thickness
// ge:keywords lake-ice ice-sheet grid swept snow-on-ice black-ice
// ge:skill mountain render
// ge:test tests/mountain_ice_look_test.cpp
#include "mountain/app/ice_look.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "core/noise.hpp"

namespace ge::mountain::app {

namespace {

using Rgb = std::array<float, 3>;
constexpr Rgb kSnowRgb{0.86f, 0.88f, 0.92f};
constexpr Rgb kBareRgb{0.10f, 0.14f, 0.18f};    // old ice: a grey-blue glass, bubbles in it
constexpr Rgb kBlackRgb{0.02f, 0.03f, 0.04f};   // young ice: the deep water seen through it
constexpr Rgb kSlushRgb{0.64f, 0.67f, 0.70f};   // thawing snow on it: grey, wet
constexpr Rgb kRottenRgb{0.06f, 0.08f, 0.09f};  // thawing ice: dark, candled, water in it
constexpr double kPatch = 40;                   // m: the swept patches' noise period

// Whether triangle a-b-c (wound as the grid's cells are) faces up with some area to it.
bool faces_up(const float* a, const float* b, const float* c) {
  const double ux = b[0] - a[0], uz = b[2] - a[2], vx = c[0] - a[0], vz = c[2] - a[2];
  return uz * vx - ux * vz > 1e-4;
}

// The point of the shore nearest (x, z), and the inward unit normal there.
struct Near {
  double x, z, d2, nx, nz;
};

Near nearest(std::span<const water::Point> shore, double x, double z) {
  Near best{x, z, 1e300, 0, 0};
  for (size_t i = 0; i < shore.size(); ++i) {
    const water::Point& a = shore[i];
    const water::Point& b = shore[(i + 1) % shore.size()];
    const double ex = b.x - a.x, ez = b.z - a.z, len2 = ex * ex + ez * ez;
    const double t = len2 > 0 ? std::clamp(((x - a.x) * ex + (z - a.z) * ez) / len2, 0.0, 1.0) : 0;
    const double px = a.x + t * ex, pz = a.z + t * ez;
    const double d2 = (x - px) * (x - px) + (z - pz) * (z - pz);
    if (d2 < best.d2) {
      const double len = std::sqrt(len2);
      best = {px, pz, d2, len > 0 ? -ez / len : 0, len > 0 ? ex / len : 0};
    }
  }
  // The normal's sign: the side the water is on.
  if (!water::inside(shore, best.x + 0.01 * best.nx, best.z + 0.01 * best.nz)) {
    best.nx = -best.nx;
    best.nz = -best.nz;
  }
  return best;
}

float ramp(double v, double a, double b) {
  return static_cast<float>(std::clamp((v - a) / (b - a), 0.0, 1.0));
}

render::Surface mix(const Rgb& from, const Rgb& to, float w, float rough) {
  render::Surface s;
  for (size_t c = 0; c < 3; ++c) s.albedo[c] = from[c] + (to[c] - from[c]) * w;
  s.rough = rough;
  s.spec = 0.5f;
  s.porous = 0;
  return s;
}

}  // namespace

render::MeshData ice_mesh(std::span<const water::Point> shore, double level, uint32_t seed,
                          bool swept_only, double inset) {
  render::MeshData m;
  if (shore.size() < 3) return m;
  double x0 = shore[0].x, z0 = shore[0].z, x1 = x0, z1 = z0;
  for (const water::Point& p : shore) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    z0 = std::min(z0, p.z);
    z1 = std::max(z1, p.z);
  }
  const auto y = static_cast<float>(level + kIceLift);
  const double cell = swept_only ? kIceCellFlat : kIceCell;
  const int cols = static_cast<int>(std::ceil((x1 - x0) / cell)) + 1;
  const int rows = static_cast<int>(std::ceil((z1 - z0) / cell)) + 1;
  const double noise_seed = static_cast<double>(seed % 100000u);
  // Every corner of the grid: whether it lies on the sheet (inside the shore and `inset` off it),
  // and where it is drawn (pulled in to the sheet's edge when it does not).
  const size_t across = static_cast<size_t>(cols) + 1;
  std::vector<uint8_t> on((static_cast<size_t>(rows) + 1) * across);
  std::vector<std::array<float, 2>> at(on.size());
  for (int row = 0; row <= rows; ++row) {
    for (int col = 0; col <= cols; ++col) {
      const double x = x0 + cell * (col - 0.5), z = z0 + cell * (row - 0.5);
      const size_t k = static_cast<size_t>(row) * across + static_cast<size_t>(col);
      const bool in = water::inside(shore, x, z);
      const Near n = in && inset <= 0 ? Near{x, z, 0, 0, 0} : nearest(shore, x, z);
      on[k] = in && (inset <= 0 || n.d2 >= inset * inset) ? 1 : 0;
      const double px = on[k] ? x : n.x + inset * n.nx, pz = on[k] ? z : n.z + inset * n.nz;
      at[k] = {static_cast<float>(px), static_cast<float>(pz)};
    }
  }
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const size_t ka = static_cast<size_t>(row) * across + static_cast<size_t>(col);
      const size_t kb = ka + across, kc = ka + 1, kd = kb + 1;
      if (!on[ka] && !on[kb] && !on[kc] && !on[kd]) continue;
      const double cx = x0 + cell * col, cz = z0 + cell * row;
      const double swept = core::value_noise(cx, cz, kPatch, noise_seed);
      const uint8_t part = swept > (swept_only ? kSweptFlat : kSwept) ? kIceBare : kIceSnow;
      if (swept_only && part != kIceBare) continue;
      const float a[3] = {at[ka][0], y, at[ka][1]};
      const float b[3] = {at[kb][0], y, at[kb][1]};
      const float c[3] = {at[kc][0], y, at[kc][1]};
      const float d[3] = {at[kd][0], y, at[kd][1]};
      // A corner drawn in can fold a triangle onto its edge or over: those are left out.
      if (faces_up(a, b, c)) render::add_triangle(m, a, b, c, part);
      if (faces_up(c, b, d)) render::add_triangle(m, c, b, d, part);
    }
  }
  return m;
}

std::vector<render::Surface> ice_surfaces(double h, double thaw) {
  // Clear black ice grows into old grey-blue ice; the snow lies on all but the swept patches.
  const float aged = ramp(h, kIceYoung, kIceSnowed);
  render::Surface bare = mix(kBlackRgb, kBareRgb, aged, 0.05f + 0.15f * aged);
  render::Surface snow = mix(kBlackRgb, kSnowRgb, ramp(h, kIceYoung, kIceSnowed), 0.05f);
  snow.rough = 0.05f + 0.95f * aged;
  snow.spec = 0.5f * (1 - aged);
  // Thawing: the snow to wet grey slush (a sheen, not a mirror, so the moat's water reads apart
  // from it), the glass to dark rotten ice, glossy with meltwater.
  const auto w = static_cast<float>(std::clamp(thaw, 0.0, 1.0));
  if (w > 0) {
    const Rgb snow_now{snow.albedo[0], snow.albedo[1], snow.albedo[2]};
    const Rgb bare_now{bare.albedo[0], bare.albedo[1], bare.albedo[2]};
    const render::Surface slush =
        mix(snow_now, kSlushRgb, w, snow.rough + (0.45f - snow.rough) * w);
    const render::Surface rotten =
        mix(bare_now, kRottenRgb, w, bare.rough + (0.05f - bare.rough) * w);
    for (size_t c = 0; c < 3; ++c) {
      snow.albedo[c] = slush.albedo[c];
      bare.albedo[c] = rotten.albedo[c];
    }
    snow.rough = slush.rough;
    snow.spec = snow.spec + (0.3f - snow.spec) * w;
    bare.rough = rotten.rough;
  }
  return {snow, bare};
}

}  // namespace ge::mountain::app
