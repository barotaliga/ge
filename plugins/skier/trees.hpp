// THE TRUNKS A SKIER CAN MEET, indexed once a map: a uniform grid over the world, each tree filed
// in the cell its trunk stands in (compressed rows: `starts` then `items`), so a step asks the
// nine cells round the skier instead of every tree on the mountain. Built at create time,
// read-only after, and free of any order but the level's own (a cell lists its trees in the
// order the level does).
//
// ge:summary The trunks indexed on a grid once a map, and the nearest one a skier overlaps
// ge:keywords trees trunk tree-index grid spatial-index collision overlap build_tree_index
// ge:keywords tree_overlap for_trees_near
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
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

TreeIndex build_tree_index(const mountain::mapgen::Level& level);

// Every tree filed in the 3x3 cells of the index round (x, z), in the index's order: every trunk
// and crown within a cell (`cell` m) of the point. `f(i)` gets each tree's index in level.trees.
template <class F>
void for_trees_near(const TreeIndex& ix, double x, double z, F&& f) {
  if (ix.cols == 0) return;
  const auto cell = [&ix](double v, int32_t n) {
    const double c = std::floor(v / ix.cell);
    if (!(c >= 0)) return int32_t{0};
    return c >= n - 1 ? n - 1 : static_cast<int32_t>(c);
  };
  const int32_t cx = cell(x, ix.cols);
  const int32_t cz = cell(z, ix.rows);
  for (int32_t r = cz - 1; r <= cz + 1; r++) {
    if (r < 0 || r >= ix.rows) continue;
    for (int32_t c = cx - 1; c <= cx + 1; c++) {
      if (c < 0 || c >= ix.cols) continue;
      const size_t at =
          static_cast<size_t>(r) * static_cast<size_t>(ix.cols) + static_cast<size_t>(c);
      for (int32_t k = ix.starts[at]; k < ix.starts[at + 1]; k++)
        f(ix.items[static_cast<size_t>(k)]);
    }
  }
}

// The tree whose trunk a body of `radius` m at (x, z) overlaps most deeply, or -1.
int32_t tree_overlap(const TreeIndex& index, const mountain::mapgen::Level& level, double x,
                     double z, double radius);

}  // namespace ge::skier
