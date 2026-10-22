// The tree index's build and its query (trees.hpp).
//
// ge:summary Builds the tree grid and answers which trunk a body overlaps
// ge:keywords trees tree-index grid build_tree_index tree_overlap
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#include "skier/trees.hpp"

#include <cmath>

#include "core/math.hpp"

namespace ge::skier {

TreeIndex build_tree_index(const mountain::mapgen::Level& level) {
  TreeIndex ix;
  const double size = level.size > 0 ? level.size : 1;
  ix.cols = static_cast<int32_t>(std::ceil(size / ix.cell));
  ix.rows = ix.cols;
  const size_t n = static_cast<size_t>(ix.cols) * static_cast<size_t>(ix.rows);
  ix.starts.assign(n + 1, 0);
  std::vector<size_t> cell_of_tree(level.trees.size());
  for (size_t i = 0; i < level.trees.size(); i++) {
    const auto& t = level.trees[i];
    const size_t c =
        static_cast<size_t>(tree_cell(t.z, ix.cell, ix.rows)) * static_cast<size_t>(ix.cols) +
        static_cast<size_t>(tree_cell(t.x, ix.cell, ix.cols));
    cell_of_tree[i] = c;
    ix.starts[c + 1]++;
  }
  for (size_t c = 0; c < n; c++) ix.starts[c + 1] += ix.starts[c];
  ix.items.assign(level.trees.size(), 0);
  std::vector<int32_t> fill(ix.starts.begin(), ix.starts.end() - 1);
  for (size_t i = 0; i < level.trees.size(); i++) {
    ix.items[static_cast<size_t>(fill[cell_of_tree[i]]++)] = static_cast<int32_t>(i);
  }
  return ix;
}

int32_t tree_overlap(const TreeIndex& ix, const mountain::mapgen::Level& level, double x, double z,
                     double radius) {
  int32_t best = -1;
  double deepest = 0;
  for_each_tree_near(ix, x, z, [&](int32_t i) {
    const auto& t = level.trees[static_cast<size_t>(i)];
    const double depth = t.radius + radius - core::hypot(x - t.x, z - t.z);
    if (depth > deepest) {
      deepest = depth;
      best = i;
    }
  });
  return best;
}

}  // namespace ge::skier
