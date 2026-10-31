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

#include "core/noise.hpp"

namespace ge::mountain::app {

namespace {

using Rgb = std::array<float, 3>;
constexpr Rgb kSnowRgb{0.86f, 0.88f, 0.92f};
constexpr Rgb kBareRgb{0.10f, 0.14f, 0.18f};   // old ice: a grey-blue glass, bubbles in it
constexpr Rgb kBlackRgb{0.02f, 0.03f, 0.04f};  // young ice: the deep water seen through it
constexpr double kPatch = 40;                  // m: the swept patches' noise period

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

render::MeshData ice_mesh(std::span<const water::Point> shore, double level, uint32_t seed) {
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
  const int cols = static_cast<int>(std::ceil((x1 - x0) / kIceCell)) + 1;
  const int rows = static_cast<int>(std::ceil((z1 - z0) / kIceCell)) + 1;
  const double noise_seed = static_cast<double>(seed % 100000u);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const double ax = x0 + kIceCell * (col - 0.5), az = z0 + kIceCell * (row - 0.5);
      const double bx = ax + kIceCell, bz = az + kIceCell;
      if (!water::inside(shore, ax, az) && !water::inside(shore, bx, az) &&
          !water::inside(shore, ax, bz) && !water::inside(shore, bx, bz)) {
        continue;
      }
      const double swept = core::value_noise(0.5 * (ax + bx), 0.5 * (az + bz), kPatch, noise_seed);
      const uint8_t part = swept > kSwept ? kIceBare : kIceSnow;
      const float a[3] = {static_cast<float>(ax), y, static_cast<float>(az)};
      const float b[3] = {static_cast<float>(ax), y, static_cast<float>(bz)};
      const float c[3] = {static_cast<float>(bx), y, static_cast<float>(az)};
      const float d[3] = {static_cast<float>(bx), y, static_cast<float>(bz)};
      render::add_triangle(m, a, b, c, part);
      render::add_triangle(m, c, b, d, part);
    }
  }
  return m;
}

std::vector<render::Surface> ice_surfaces(double h) {
  // Clear black ice grows into old grey-blue ice; the snow lies on all but the swept patches.
  const float aged = ramp(h, kIceYoung, kIceSnowed);
  const render::Surface bare = mix(kBlackRgb, kBareRgb, aged, 0.05f + 0.15f * aged);
  render::Surface snow = mix(kBlackRgb, kSnowRgb, ramp(h, kIceYoung, kIceSnowed), 0.05f);
  snow.rough = 0.05f + 0.95f * aged;
  snow.spec = 0.5f * (1 - aged);
  return {snow, bare};
}

}  // namespace ge::mountain::app
