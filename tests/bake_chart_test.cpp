// engine/bake/chart.hpp: a chart's axes run along the world where they can and close on the
// plane's normal, its luxels cover its triangles with a border, the atlas packs every chart
// without overlap, and a point's atlas place is where its luxel is.
//
// ge:summary Test: lightmap charts' axes, size and border; the atlas packing; chart_point
// ge:keywords test bake chart atlas pack luxel border plan_chart pack_charts chart_point
// ge:skill testing render
#include "bake/chart.hpp"

#include <cmath>
#include <vector>

#include "check.hpp"

namespace {

using namespace ge::bake;

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

}  // namespace

// A 4 m x 2 m floor at 0.25 m a luxel: 16 + 2 by 8 + 2 luxels, u along x, u x v up.
TEST(chart_floor_axes_and_size) {
  Scene s;
  const double floor[4][3] = {{0, 0, 0}, {0, 0, 2}, {4, 0, 2}, {4, 0, 0}};
  const uint32_t t = add_quad(s, floor, 0);
  const uint32_t tris[2] = {t, t + 1};
  const Chart c = plan_chart(s, tris, 0.25);
  CHECK(c.width == 18);
  CHECK(c.height == 10);
  CHECK(std::fabs(c.normal[1] - 1) < 1e-12);
  CHECK(std::fabs(c.u[0] - 0.25) < 1e-12);
  CHECK(std::fabs(dot3(c.u, c.v)) < 1e-12);
  const double x[3] = {c.u[1] * c.v[2] - c.u[2] * c.v[1], c.u[2] * c.v[0] - c.u[0] * c.v[2],
                       c.u[0] * c.v[1] - c.u[1] * c.v[0]};
  CHECK(dot3(x, c.normal) > 0);
  // Every corner falls a luxel or more inside the chart's edge.
  for (const auto& p : floor) {
    double si = 0, ti = 0;
    chart_point(c, p, &si, &ti);
    CHECK(si >= 1 - 1e-9 && si <= c.width - 1 + 1e-9);
    CHECK(ti >= 1 - 1e-9 && ti <= c.height - 1 + 1e-9);
  }
}

// A wall facing +x runs its u along z; one facing -z along x.
TEST(chart_wall_axes) {
  Scene s;
  const double east[4][3] = {{0, 0, 0}, {0, 0, 3}, {0, 2, 3}, {0, 2, 0}};
  const uint32_t t = add_quad(s, east, 0);
  const uint32_t tris[2] = {t, t + 1};
  const Chart c = plan_chart(s, tris, 0.5);
  CHECK(std::fabs(std::fabs(c.u[2]) - 0.5) < 1e-12);
  CHECK(std::fabs(std::fabs(c.normal[0]) - 1) < 1e-12);
}

// A group too large for kMaxChartSide at the asked size gets coarser luxels, not a huge chart.
TEST(chart_coarsens_when_too_large) {
  Scene s;
  const double big[4][3] = {{0, 0, 0}, {0, 0, 400}, {400, 0, 400}, {400, 0, 0}};
  const uint32_t t = add_quad(s, big, 0);
  const uint32_t tris[2] = {t, t + 1};
  const Chart c = plan_chart(s, tris, 0.25);
  CHECK(c.width <= kMaxChartSide && c.height <= kMaxChartSide);
}

// Many charts pack without overlap inside the atlas; a point's uv lands in its own chart.
TEST(chart_pack_no_overlap) {
  Scene s;
  for (int i = 0; i < 40; i++) {
    const double w = 0.5 + (i % 7) * 0.7, d = 0.5 + (i % 5) * 0.9;
    const double q[4][3] = {
        {i * 10.0, 0, 0}, {i * 10.0, 0, d}, {i * 10.0 + w, 0, d}, {i * 10.0 + w, 0, 0}};
    const uint32_t t = add_quad(s, q, 0);
    const uint32_t tris[2] = {t, t + 1};
    add_chart(s, tris, 0.25);
  }
  CHECK(pack_charts(s, 128));
  std::vector<int> owner(size_t{s.atlas_width} * s.atlas_height, -1);
  for (size_t c = 0; c < s.charts.size(); c++) {
    const Chart& ch = s.charts[c];
    CHECK(ch.x + ch.width <= s.atlas_width && ch.y + ch.height <= s.atlas_height);
    for (uint32_t j = 0; j < ch.height; j++) {
      for (uint32_t i = 0; i < ch.width; i++) {
        int& o = owner[(ch.y + j) * s.atlas_width + ch.x + i];
        CHECK(o == -1);
        o = static_cast<int>(c);
      }
    }
  }
  CHECK(s.atlas_height % 4 == 0);
  // The middle of quad 3 lands in chart 3's rectangle.
  const Chart& ch = s.charts[3];
  const double mid[3] = {30 + (0.5 + 3 * 0.7) / 2, 0, (0.5 + 3 * 0.9) / 2};
  float uv[2];
  chart_uv(ch, s.atlas_width, s.atlas_height, mid, uv);
  const double ax = double{uv[0]} * s.atlas_width, ay = double{uv[1]} * s.atlas_height;
  CHECK(ax > ch.x && ax < ch.x + ch.width && ay > ch.y && ay < ch.y + ch.height);
  // Wider than the atlas: refused.
  CHECK(!pack_charts(s, 8));
}
