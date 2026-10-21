// THE TRUNKS A SKIER CAN MEET, indexed once a map: a uniform grid over the world, each tree filed
// in the cell its trunk stands in (compressed rows: `starts` then `items`), so a step asks the
// nine cells round the skier instead of every tree on the mountain. Built at create time,
// read-only after, and free of any order but the level's own (a cell lists its trees in the
// order the level does).
//
// ge:summary The trunks indexed on a grid once a map, and the nearest one a skier overlaps
// ge:keywords trees trunk tree-index grid spatial-index collision overlap build_tree_index
// ge:keywords tree_overlap
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

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

// The tree whose trunk a body of `radius` m at (x, z) overlaps most deeply, or -1.
int32_t tree_overlap(const TreeIndex& index, const mountain::mapgen::Level& level, double x,
                     double z, double radius);

}  // namespace ge::skier
