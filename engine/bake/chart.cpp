// The charts' planning and packing: the plane's axes, the bounds in luxels, the shelves.
//
// ge:summary Defines plan_chart (axes, bounds, luxel size) and pack_charts (shelf packing)
// ge:keywords bake chart atlas pack plan_chart pack_charts shelf
// ge:skill render
// ge:test tests/bake_chart_test.cpp
#include "bake/chart.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace ge::bake {

namespace {

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void unit(double a[3]) {
  const double l = std::sqrt(dot3(a, a));
  if (l > 0) {
    a[0] /= l;
    a[1] /= l;
    a[2] /= l;
  }
}

void vertex(const Scene& scene, uint32_t index, double out[3]) {
  for (int k = 0; k < 3; k++) out[k] = scene.positions[size_t{index} * 3 + k];
}

}  // namespace

Chart plan_chart(const Scene& scene, std::span<const uint32_t> tris, double luxel_m) {
  Chart c;
  if (tris.empty()) return c;
  // The normal: the largest triangle's (a sliver's cross product is all rounding).
  double n[3] = {0, 1, 0}, best = -1;
  for (const uint32_t t : tris) {
    double a[3], b[3], d[3];
    vertex(scene, scene.indices[size_t{t} * 3], a);
    vertex(scene, scene.indices[size_t{t} * 3 + 1], b);
    vertex(scene, scene.indices[size_t{t} * 3 + 2], d);
    const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e2[3] = {d[0] - a[0], d[1] - a[1], d[2] - a[2]};
    double x[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                   e1[0] * e2[1] - e1[1] * e2[0]};
    const double area2 = dot3(x, x);
    if (area2 > best) {
      best = area2;
      unit(x);
      n[0] = x[0];
      n[1] = x[1];
      n[2] = x[2];
    }
  }
  // u along x where the plane allows it, else along z; v = n × u, so u × v = n.
  const double ax = std::fabs(n[0]);
  const double along[3] = {ax >= std::fabs(n[1]) && ax >= std::fabs(n[2]) ? 0.0 : 1.0, 0,
                           ax >= std::fabs(n[1]) && ax >= std::fabs(n[2]) ? 1.0 : 0.0};
  const double an = dot3(along, n);
  double u[3] = {along[0] - n[0] * an, along[1] - n[1] * an, along[2] - n[2] * an};
  unit(u);
  const double v[3] = {n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2],
                       n[0] * u[1] - n[1] * u[0]};

  double s0 = 1e300, s1 = -1e300, t0 = 1e300, t1 = -1e300, depth = 0;
  for (const uint32_t t : tris) {
    for (int k = 0; k < 3; k++) {
      double p[3];
      vertex(scene, scene.indices[size_t{t} * 3 + k], p);
      const double s = dot3(p, u), tt = dot3(p, v);
      s0 = std::min(s0, s);
      s1 = std::max(s1, s);
      t0 = std::min(t0, tt);
      t1 = std::max(t1, tt);
    }
  }
  {
    double p[3];
    vertex(scene, scene.indices[size_t{tris[0]} * 3], p);
    depth = dot3(p, n);
  }
  double lux = luxel_m > 0 ? luxel_m : 0.25;
  // Centres from a luxel outside the low edge to at least half a luxel past the high one.
  auto side = [&](double span) { return static_cast<uint32_t>(std::ceil(span / lux)) + 2; };
  while (side(s1 - s0) > kMaxChartSide || side(t1 - t0) > kMaxChartSide) lux *= 1.25;
  c.width = std::max<uint32_t>(side(s1 - s0), 3);
  c.height = std::max<uint32_t>(side(t1 - t0), 3);
  const double os = s0 - lux, ot = t0 - lux;
  for (int k = 0; k < 3; k++) {
    c.origin[k] = u[k] * os + v[k] * ot + n[k] * depth;
    c.u[k] = u[k] * lux;
    c.v[k] = v[k] * lux;
    c.normal[k] = n[k];
  }
  return c;
}

uint32_t add_chart(Scene& scene, std::span<const uint32_t> tris, double luxel_m) {
  const auto index = static_cast<uint32_t>(scene.charts.size());
  scene.charts.push_back(plan_chart(scene, tris, luxel_m));
  if (scene.tri_chart.size() < scene.triangles())
    scene.tri_chart.resize(scene.triangles(), kNoChart);
  for (const uint32_t t : tris) scene.tri_chart[t] = index;
  return index;
}

uint32_t add_quad(Scene& scene, const double corners[4][3], uint32_t material) {
  const auto base = static_cast<uint32_t>(scene.positions.size() / 3);
  for (int c = 0; c < 4; c++) {
    for (int k = 0; k < 3; k++) scene.positions.push_back(static_cast<float>(corners[c][k]));
  }
  const auto first = static_cast<uint32_t>(scene.triangles());
  for (const uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) scene.indices.push_back(base + i);
  scene.tri_material.push_back(material);
  scene.tri_material.push_back(material);
  scene.tri_chart.resize(scene.triangles(), kNoChart);
  return first;
}

bool pack_charts(Scene& scene, uint32_t width) {
  std::vector<uint32_t> order(scene.charts.size());
  std::iota(order.begin(), order.end(), 0u);
  std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return scene.charts[a].height > scene.charts[b].height;
  });
  uint32_t x = 0, y = 0, shelf = 0;
  for (const uint32_t i : order) {
    Chart& c = scene.charts[i];
    if (c.width > width) return false;
    if (x + c.width > width) {
      y += shelf;
      x = 0;
      shelf = 0;
    }
    c.x = x;
    c.y = y;
    x += c.width;
    shelf = std::max(shelf, c.height);
  }
  scene.atlas_width = width;
  scene.atlas_height = (y + shelf + 3) / 4 * 4;
  if (scene.atlas_height == 0) scene.atlas_height = 4;
  return true;
}

void chart_point(const Chart& chart, const double p[3], double* s, double* t) {
  const double d[3] = {p[0] - chart.origin[0], p[1] - chart.origin[1], p[2] - chart.origin[2]};
  *s = dot3(d, chart.u) / dot3(chart.u, chart.u);
  *t = dot3(d, chart.v) / dot3(chart.v, chart.v);
}

void chart_uv(const Chart& chart, uint32_t atlas_width, uint32_t atlas_height, const double p[3],
              float uv[2]) {
  double s = 0, t = 0;
  chart_point(chart, p, &s, &t);
  uv[0] = static_cast<float>((chart.x + s) / atlas_width);
  uv[1] = static_cast<float>((chart.y + t) / atlas_height);
}

}  // namespace ge::bake
