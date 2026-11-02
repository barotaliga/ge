// plugins/brush/app/bsp_draw.hpp's light grid (`bin_light_grid`): every light whose reach takes in
// a point is listed in that point's cell, the lists keep the lights' order, the grid sits round
// the eye, and lists past their cap turn the grid off (every pixel then reads every light).
//
// ge:summary Tests for the BSP light grid: no reaching light left out, the order kept, the cap
// ge:keywords test brush bsp light-grid light-cull bin_light_grid lights-per-pixel
// ge:skill brush testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "check.hpp"
#include "core/prng.hpp"

namespace {

namespace ba = ge::brush::app;

ba::GpuLight light_at(double x, double y, double z, double reach) {
  ba::GpuLight l{};
  l.pos[0] = float(x);
  l.pos[1] = float(y);
  l.pos[2] = float(z);
  l.pos[3] = float(reach);
  return l;
}

// The cell `p` lies in, as bsp.wgsl's grid_cell finds it; -1 outside.
int cell_of(const float grid[4], const double p[3]) {
  const uint32_t dims[3] = {ba::kGridX, ba::kGridY, ba::kGridZ};
  int c[3];
  for (int k = 0; k < 3; ++k) {
    c[k] = int(std::floor((p[k] - grid[k]) / grid[3]));
    if (c[k] < 0 || c[k] >= int(dims[k])) return -1;
  }
  return c[0] + int(ba::kGridX) * (c[1] + int(ba::kGridY) * c[2]);
}

}  // namespace

TEST(every_light_that_reaches_a_point_is_in_its_cells_list_in_order) {
  ge::core::Rng rng = ge::core::make_rng(38);
  const double eye[3] = {3.3, 1.7, -12.9};
  std::vector<ba::GpuLight> lights;
  lights.reserve(160);
  for (int i = 0; i < 160; ++i)
    lights.push_back(light_at(eye[0] + ge::core::range(rng, -50, 50),
                              eye[1] + ge::core::range(rng, -20, 20),
                              eye[2] + ge::core::range(rng, -50, 50), ge::core::range(rng, 1, 14)));
  std::vector<uint32_t> rows;
  float grid[4];
  const uint32_t refs = ba::bin_light_grid(lights, eye, rows, grid);
  CHECK(grid[3] == float(ba::kGridCell));
  CHECK(refs > 0 && refs == rows[ba::kGridCells]);
  // The grid is centred on the eye's cell.
  for (int k = 0; k < 3; ++k) CHECK(eye[k] > grid[k] && eye[k] < grid[k] + 32 * ba::kGridCell);
  const uint32_t* list = rows.data() + ba::kGridCells + 1;
  int inside = 0;
  for (int n = 0; n < 20000; ++n) {
    const double p[3] = {eye[0] + ge::core::range(rng, -70, 70),
                         eye[1] + ge::core::range(rng, -35, 35),
                         eye[2] + ge::core::range(rng, -70, 70)};
    const int c = cell_of(grid, p);
    if (c < 0) continue;
    ++inside;
    const uint32_t first = rows[size_t(c)], end = rows[size_t(c) + 1];
    CHECK(first <= end);
    for (uint32_t j = first + 1; j < end; ++j) CHECK(list[j - 1] < list[j]);
    for (uint32_t i = 0; i < lights.size(); ++i) {
      double d2 = 0;
      for (int k = 0; k < 3; ++k) d2 += (p[k] - lights[i].pos[k]) * (p[k] - lights[i].pos[k]);
      if (d2 >= double(lights[i].pos[3]) * lights[i].pos[3]) continue;
      bool listed = false;
      for (uint32_t j = first; j < end; ++j) listed = listed || list[j] == i;
      CHECK(listed);
    }
  }
  CHECK(inside > 5000);
}

TEST(lists_past_their_cap_turn_the_grid_off) {
  const double eye[3] = {0, 0, 0};
  // Lights that cover the whole grid, until the lists would pass the cap.
  const uint32_t each = ba::kGridCells;
  std::vector<ba::GpuLight> lights(ba::kMaxGridRefs / each + 1, light_at(0, 0, 0, 1000));
  std::vector<uint32_t> rows;
  float grid[4];
  CHECK(ba::bin_light_grid(lights, eye, rows, grid) == 0);
  CHECK(grid[3] == 0);
  lights.pop_back();
  CHECK(ba::bin_light_grid(lights, eye, rows, grid) == ba::kMaxGridRefs);
  CHECK(grid[3] > 0);
}
