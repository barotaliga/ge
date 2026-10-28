// THE TRUNKS A SKIER CAN MEET, indexed once a map: a uniform grid over the world, each tree filed
// in the cell its trunk stands in (compressed rows: `starts` then `items`), so a step asks the
// nine cells round the skier instead of every tree on the mountain. Built at create time,
// read-only after, and free of any order but the level's own (a cell lists its trees in the
// order the level does).
//
// ge:summary The trunks indexed on a grid once a map, and the nearest one a skier overlaps
// ge:keywords trees trunk tree-index grid spatial-index collision overlap build_tree_index
// ge:keywords tree_overlap for_each_tree_near
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::skier {

struct TreeIndex {
  double cell = 16;  // m
  int32_t cols = 0;
  int32_t rows = 0;
  std::vector<int32_t> starts;  // cols * rows + 1 offsets into `items`
  std::vector<int32_t> items;   // indices into level.trees
};

// `gone` (empty, or one flag a tree of `level.trees`) leaves out every tree flagged 1: felled.
TreeIndex build_tree_index(const mountain::mapgen::Level& level,
                           std::span<const uint8_t> gone = {});

// The tree whose trunk a body of `radius` m at (x, z) overlaps most deeply, or -1.
int32_t tree_overlap(const TreeIndex& index, const mountain::mapgen::Level& level, double x,
                     double z, double radius);

// The cell (x, z) is filed in, clamped onto the grid.
inline int32_t tree_cell(double v, double cell, int32_t n) {
  const double c = std::floor(v / cell);
  if (!(c >= 0)) return 0;
  if (c >= n - 1) return n - 1;
  return static_cast<int32_t>(c);
}

// Call `visit(i)` for every tree filed in the three by three cells round (x, z): every trunk
// within one cell (`TreeIndex::cell` m) of it, and some further. Allocates nothing.
template <class F>
void for_each_tree_near(const TreeIndex& ix, double x, double z, F&& visit) {
  if (ix.cols == 0) return;
  const int32_t cx = tree_cell(x, ix.cell, ix.cols);
  const int32_t cz = tree_cell(z, ix.cell, ix.rows);
  for (int32_t r = cz - 1; r <= cz + 1; r++) {
    if (r < 0 || r >= ix.rows) continue;
    for (int32_t c = cx - 1; c <= cx + 1; c++) {
      if (c < 0 || c >= ix.cols) continue;
      const size_t cell =
          static_cast<size_t>(r) * static_cast<size_t>(ix.cols) + static_cast<size_t>(c);
      for (int32_t k = ix.starts[cell]; k < ix.starts[cell + 1]; k++) {
        visit(ix.items[static_cast<size_t>(k)]);
      }
    }
  }
}

}  // namespace ge::skier
