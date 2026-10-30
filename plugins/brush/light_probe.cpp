// The baked light under a point (light_probe.hpp).
//
// ge:summary The baked light on the floor under a point, off the map's lightmaps
// ge:keywords light-probe light_below make_light_probe
// ge:skill brush
#include "brush/light_probe.hpp"

#include <algorithm>
#include <cmath>

namespace ge::brush {

namespace {

constexpr float kUpward = 0.1f;  // a face's normal z at least this to count as a floor

// The triangle at `first` (into Map::indices) as three positions.
void corners(const Map& map, uint32_t first, const float* c[3]) {
  for (int k = 0; k < 3; ++k) c[k] = map.vertices[map.indices[first + k]].pos;
}

bool upward(const float* c[3]) {
  const float ax = c[1][0] - c[0][0], ay = c[1][1] - c[0][1], az = c[1][2] - c[0][2];
  const float bx = c[2][0] - c[0][0], by = c[2][1] - c[0][1], bz = c[2][2] - c[0][2];
  const float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
  const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
  return len > 0 && std::fabs(nz) / len >= kUpward;
}

// The cell of a coordinate, clamped to the grid.
int cell_of(float v, float lo, int n) {
  return std::clamp(static_cast<int>(std::floor((v - lo) / kProbeCell)), 0, n - 1);
}

// One luxel's channel `c` (0..1), clamped to the lightmap.
float luxel(const Map& map, const Lightmap& lm, int x, int y, int c) {
  x = std::clamp(x, 0, static_cast<int>(lm.width) - 1);
  y = std::clamp(y, 0, static_cast<int>(lm.height) - 1);
  const size_t at = lm.offset + (static_cast<size_t>(y) * lm.width + static_cast<size_t>(x)) * 3;
  return at + 2 < map.light_rgb.size() ? map.light_rgb[at + static_cast<size_t>(c)] / 255.0f : 0;
}

}  // namespace

LightProbe make_light_probe(const Map& map) {
  LightProbe p;
  if (map.models.empty() || map.lightmaps.empty()) return p;
  const Model& world = map.models[0];
  std::vector<uint32_t> firsts;
  std::vector<int32_t> maps;
  float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
  for (uint32_t s = world.first_surface; s < world.first_surface + world.surface_count; ++s) {
    if (s >= map.surfaces.size()) break;
    const Surface& sf = map.surfaces[s];
    if (sf.lightmap < 0 || sf.kind == SurfaceKind::sky || sf.kind == SurfaceKind::liquid) continue;
    for (uint32_t i = sf.first; i + 2 < sf.first + sf.count; i += 3) {
      const float* c[3];
      corners(map, i, c);
      if (!upward(c)) continue;
      firsts.push_back(i);
      maps.push_back(sf.lightmap);
      for (int k = 0; k < 3; ++k)
        for (int a = 0; a < 2; ++a) {
          lo[a] = std::min(lo[a], c[k][a]);
          hi[a] = std::max(hi[a], c[k][a]);
        }
    }
  }
  if (firsts.empty()) return p;
  p.lo[0] = lo[0];
  p.lo[1] = lo[1];
  p.nx = std::max(1, static_cast<int>(std::ceil((hi[0] - lo[0]) / kProbeCell)));
  p.ny = std::max(1, static_cast<int>(std::ceil((hi[1] - lo[1]) / kProbeCell)));
  const size_t cells = static_cast<size_t>(p.nx) * static_cast<size_t>(p.ny);
  // Two passes: count a cell's triangles, then place them.
  std::vector<uint32_t> count(cells + 1, 0);
  const auto each_cell = [&](uint32_t first, auto&& fn) {
    const float* c[3];
    corners(map, first, c);
    const int x0 = cell_of(std::min({c[0][0], c[1][0], c[2][0]}), p.lo[0], p.nx);
    const int x1 = cell_of(std::max({c[0][0], c[1][0], c[2][0]}), p.lo[0], p.nx);
    const int y0 = cell_of(std::min({c[0][1], c[1][1], c[2][1]}), p.lo[1], p.ny);
    const int y1 = cell_of(std::max({c[0][1], c[1][1], c[2][1]}), p.lo[1], p.ny);
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) fn(static_cast<size_t>(y) * static_cast<size_t>(p.nx) + x);
  };
  for (uint32_t f : firsts) each_cell(f, [&](size_t k) { ++count[k + 1]; });
  for (size_t k = 0; k < cells; ++k) count[k + 1] += count[k];
  p.start = count;
  p.tris.resize(count[cells]);
  p.lightmap.resize(count[cells]);
  for (size_t t = 0; t < firsts.size(); ++t)
    each_cell(firsts[t], [&](size_t k) {
      p.tris[count[k]] = firsts[t];
      p.lightmap[count[k]] = maps[t];
      ++count[k];
    });
  return p;
}

bool light_below(const LightProbe& probe, const Map& map, const float p[3], float rgb[3],
                 float reach) {
  if (probe.nx == 0 || probe.ny == 0) return false;
  const float gx = (p[0] - probe.lo[0]) / kProbeCell, gy = (p[1] - probe.lo[1]) / kProbeCell;
  if (gx < 0 || gy < 0 || gx >= static_cast<float>(probe.nx) || gy >= static_cast<float>(probe.ny))
    return false;
  const size_t k = static_cast<size_t>(static_cast<int>(gy)) * static_cast<size_t>(probe.nx) +
                   static_cast<size_t>(static_cast<int>(gx));
  float best_z = p[2] - reach;
  bool found = false;
  float uv[2] = {0, 0};
  int32_t best_map = -1;
  for (uint32_t e = probe.start[k]; e < probe.start[k + 1]; ++e) {
    const uint32_t first = probe.tris[e];
    const float* c[3];
    corners(map, first, c);
    // The point's barycentric weights in the triangle seen from above.
    const float d =
        (c[1][1] - c[2][1]) * (c[0][0] - c[2][0]) + (c[2][0] - c[1][0]) * (c[0][1] - c[2][1]);
    if (std::fabs(d) < 1e-6f) continue;
    const float w0 =
        ((c[1][1] - c[2][1]) * (p[0] - c[2][0]) + (c[2][0] - c[1][0]) * (p[1] - c[2][1])) / d;
    const float w1 =
        ((c[2][1] - c[0][1]) * (p[0] - c[2][0]) + (c[0][0] - c[2][0]) * (p[1] - c[2][1])) / d;
    const float w2 = 1 - w0 - w1;
    constexpr float kEdge = -1e-4f;
    if (w0 < kEdge || w1 < kEdge || w2 < kEdge) continue;
    const float z = w0 * c[0][2] + w1 * c[1][2] + w2 * c[2][2];
    if (z > p[2] + 1 || z <= best_z) continue;
    best_z = z;
    found = true;
    best_map = probe.lightmap[e];
    const float* l[3];
    for (int i = 0; i < 3; ++i) l[i] = map.vertices[map.indices[first + i]].lm;
    uv[0] = w0 * l[0][0] + w1 * l[1][0] + w2 * l[2][0];
    uv[1] = w0 * l[0][1] + w1 * l[1][1] + w2 * l[2][1];
  }
  if (!found || best_map < 0 || static_cast<size_t>(best_map) >= map.lightmaps.size()) return false;
  // `lm` is (luxel + 0.5) / size across the lightmap: back to luxels, then bilinear.
  const Lightmap& lm = map.lightmaps[static_cast<size_t>(best_map)];
  const float fx = uv[0] * static_cast<float>(lm.width) - 0.5f;
  const float fy = uv[1] * static_cast<float>(lm.height) - 0.5f;
  const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
  const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
  for (int ch = 0; ch < 3; ++ch) {
    const float a = luxel(map, lm, x0, y0, ch) * (1 - tx) + luxel(map, lm, x0 + 1, y0, ch) * tx;
    const float b =
        luxel(map, lm, x0, y0 + 1, ch) * (1 - tx) + luxel(map, lm, x0 + 1, y0 + 1, ch) * tx;
    rgb[ch] = a * (1 - ty) + b * ty;
  }
  return true;
}

}  // namespace ge::brush
