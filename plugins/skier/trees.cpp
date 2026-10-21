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

namespace {

int32_t cell_of(double v, double cell, int32_t n) {
  const double c = std::floor(v / cell);
  if (!(c >= 0)) return 0;
  if (c >= n - 1) return n - 1;
  return static_cast<int32_t>(c);
}

}  // namespace

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
        static_cast<size_t>(cell_of(t.z, ix.cell, ix.rows)) * static_cast<size_t>(ix.cols) +
        static_cast<size_t>(cell_of(t.x, ix.cell, ix.cols));
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
  if (ix.cols == 0) return -1;
  const int32_t cx = cell_of(x, ix.cell, ix.cols);
  const int32_t cz = cell_of(z, ix.cell, ix.rows);
  int32_t best = -1;
  double deepest = 0;
  for (int32_t r = cz - 1; r <= cz + 1; r++) {
    if (r < 0 || r >= ix.rows) continue;
    for (int32_t c = cx - 1; c <= cx + 1; c++) {
      if (c < 0 || c >= ix.cols) continue;
      const size_t cell =
          static_cast<size_t>(r) * static_cast<size_t>(ix.cols) + static_cast<size_t>(c);
      for (int32_t k = ix.starts[cell]; k < ix.starts[cell + 1]; k++) {
        const int32_t i = ix.items[static_cast<size_t>(k)];
        const auto& t = level.trees[static_cast<size_t>(i)];
        const double depth = t.radius + radius - core::hypot(x - t.x, z - t.z);
        if (depth > deepest) {
          deepest = depth;
          best = i;
        }
      }
    }
  }
  return best;
}

}  // namespace ge::skier
