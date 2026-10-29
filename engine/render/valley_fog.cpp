// ge:summary The valley fog's frame rows and its closed-form depth along a ray (the CPU's half)
// ge:keywords render valley-fog valley_rows valley_depth optical-depth
// ge:skill render
// ge:test tests/render_valley_fog_test.cpp
#include "render/valley_fog.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

void valley_rows(const ValleyFog& fog, const FrameView& view, float rows[2][4]) {
  for (int r = 0; r < 2; ++r) {
    for (int c = 0; c < 4; ++c) rows[r][c] = 0.0f;
  }
  if (!(fog.density > 0)) return;
  const double size = std::max(fog.wave_size_m, 1.0);
  rows[0][0] = static_cast<float>(fog.top_m - view.eye[1]);
  rows[0][1] = static_cast<float>(fog.density);
  rows[0][2] = static_cast<float>(std::max(fog.edge_m, 1e-3));
  rows[0][3] = static_cast<float>(fog.wave_m);
  rows[1][0] = static_cast<float>(1.0 / size);
  // The offset in doubles, then its share of the tile: the shader adds the camera-relative
  // point's share, which stays small.
  const double x = (view.eye[0] - fog.drift[0] * view.time_s) / size;
  const double z = (view.eye[2] - fog.drift[1] * view.time_s) / size;
  rows[1][1] = static_cast<float>(x - std::floor(x));
  rows[1][2] = static_cast<float>(z - std::floor(z));
  rows[1][3] = static_cast<float>(std::clamp(fog.albedo, 0.0, 1.0));
}

double valley_share(double top, double edge, double y) {
  return std::clamp((top + 0.5 * edge - y) / edge, 0.0, 1.0);
}

double valley_column(double top, double edge, double y) {
  const double hi = top + 0.5 * edge;
  if (y >= hi) return 0;
  if (y >= top - 0.5 * edge) return (hi - y) * (hi - y) / (2 * edge);
  return top - y;
}

double valley_depth(double top, double edge, double density, double dy, double dist) {
  const double end = dy * dist;
  if (std::fabs(dy) < 1e-4) return density * dist * valley_share(top, edge, 0.5 * end);
  return density * std::fabs(valley_column(top, edge, 0) - valley_column(top, edge, end)) /
         std::fabs(dy);
}

}  // namespace ge::render
