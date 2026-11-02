// The BSP's light grid binned (light_grid.hpp).
//
// ge:summary The BSP's light grid: each light's reach as runs of cells, counted, then placed
// ge:keywords bsp brush light-grid bin_light_grid runs
// ge:skill brush render
// ge:test tests/brush_bsp_light_grid_test.cpp
#include "brush/app/light_grid.hpp"

#include <algorithm>
#include <cmath>

namespace ge::brush::app {

namespace {

void set4(float out[4], double a, double b, double c, double d) {
  out[0] = float(a);
  out[1] = float(b);
  out[2] = float(c);
  out[3] = float(d);
}

// A cell lists a light when the light's reach comes within this of its box, m: more than the
// shader's rounding of a point into its cell or of its distance, so no light a pixel would add is
// left out.
constexpr double kGridMargin = 0.05;

}  // namespace

// Two walks over the same runs of cells, the first counting, the second placing, so each list
// keeps the lights' order.

uint32_t bin_light_grid(std::span<const GpuLight> lights, const double eye[3],
                        std::vector<uint32_t>& rows, float grid[4]) {
  const uint32_t dims[3] = {kGridX, kGridY, kGridZ};
  double corner[3];
  for (int k = 0; k < 3; ++k)
    corner[k] = (std::floor(eye[k] / kGridCell) - double(dims[k]) / 2) * kGridCell;
  if (rows.size() != size_t(kGridCells) + 1 + kMaxGridRefs)
    rows.assign(size_t(kGridCells) + 1 + kMaxGridRefs, 0);
  std::fill(rows.begin(), rows.begin() + kGridCells + 1, 0u);
  // Each run of cells along x that light `l`'s reach touches: `visit(first cell, last cell)`. In a
  // row (y, z) the reach left over past the row's gap is a half width along x, and the cells it
  // spans are one run, so no cell is tested alone.
  const auto walk = [&](const GpuLight& l, auto&& visit) {
    const double r = double(l.pos[3]) + kGridMargin;
    const double c[3] = {double(l.pos[0]), double(l.pos[1]), double(l.pos[2])};
    int lo[3], hi[3];
    for (int k = 0; k < 3; ++k) {
      lo[k] = std::max(int(std::floor((c[k] - r - corner[k]) / kGridCell)), 0);
      hi[k] = std::min(int(std::floor((c[k] + r - corner[k]) / kGridCell)), int(dims[k]) - 1);
      if (lo[k] > hi[k]) return;
    }
    // How far `c` lies outside cell `i`'s span along axis `k`, m.
    const auto gap = [&](int k, int i) {
      const double a = corner[k] + i * kGridCell, b = a + kGridCell;
      return c[k] < a ? a - c[k] : c[k] > b ? c[k] - b : 0.0;
    };
    for (int z = lo[2]; z <= hi[2]; ++z) {
      const double gz = gap(2, z);
      for (int y = lo[1]; y <= hi[1]; ++y) {
        const double gy = gap(1, y);
        const double left = r * r - gz * gz - gy * gy;
        if (left < 0) continue;
        const double half = std::sqrt(left);
        const int x0 = std::max(int(std::floor((c[0] - half - corner[0]) / kGridCell)), lo[0]);
        const int x1 = std::min(int(std::floor((c[0] + half - corner[0]) / kGridCell)), hi[0]);
        if (x0 > x1) continue;
        const uint32_t row = kGridX * (uint32_t(y) + kGridY * uint32_t(z));
        visit(row + uint32_t(x0), row + uint32_t(x1));
      }
    }
  };
  for (const GpuLight& l : lights)
    walk(l, [&](uint32_t c0, uint32_t c1) {
      for (uint32_t c = c0; c <= c1; ++c) ++rows[c + 1];
    });
  for (uint32_t c = 0; c < kGridCells; ++c) rows[c + 1] += rows[c];
  const uint32_t refs = rows[kGridCells];
  if (refs > kMaxGridRefs) {
    set4(grid, 0, 0, 0, 0);
    return 0;
  }
  // Placing: each cell's start serves as its next entry and ends at the next cell's start, so the
  // starts are shifted back by one cell afterwards.
  uint32_t* const list = rows.data() + kGridCells + 1;
  for (uint32_t i = 0; i < lights.size(); ++i)
    walk(lights[i], [&](uint32_t c0, uint32_t c1) {
      for (uint32_t c = c0; c <= c1; ++c) list[rows[c]++] = i;
    });
  for (uint32_t c = kGridCells; c > 0; --c) rows[c] = rows[c - 1];
  rows[0] = 0;
  set4(grid, corner[0], corner[1], corner[2], kGridCell);
  return refs;
}

}  // namespace ge::brush::app
