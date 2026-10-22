// The woods by height: the region's ecotone rows and the two curves read off them.
//
// ge:summary The region rows of a ski area's woods by height and the curves read off them
// ge:keywords resort-woods woods-row woods-at-depth tall-at-depth ecotone thin-trees
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/resort_woods.hpp"

#include <array>
#include <cmath>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::mountain::mapgen {

namespace {

// The ecotones by region, in RegionId's order: the alpine's half cover seventy metres under the
// line, the fell's abrupt birch edge, the continental's wide band and the maritime's parkland.
constexpr std::array<WoodsRow, 4> kWoods{{
    {70, 22, 260, 250},
    {40, 15, 160, 160},
    {100, 35, 320, 280},
    {120, 40, 300, 260},
}};

}  // namespace

const WoodsRow& woods_row(RegionId id) { return kWoods[static_cast<size_t>(id)]; }

WoodsCover woods_at_depth(const WoodsRow& row, double d) {
  if (d <= 0) return {0, 0};
  const double keep = 1 / (1 + ge::core::libm::exp(-(d - row.mid) / row.steep));
  const double close = 0.75 * ge::core::smoothstep(row.mid * 2, row.closed, d);
  return {keep, close};
}

double tall_at_depth(const WoodsRow& row, double d) {
  if (d <= 0) return 0;
  return 1 - ge::core::libm::exp(-d / row.reach) * 0.92;
}

void thin_trees(std::vector<TreeDef>& trees, size_t most) {
  if (trees.size() <= most) return;
  const double golden = (std::sqrt(5.0) - 1) / 2;
  const double keep = double(most) / double(trees.size());
  std::vector<TreeDef> kept;
  kept.reserve(most);
  for (size_t i = 0; i < trees.size() && kept.size() < most; i++) {
    const double at = double(i + 1) * golden;
    if (at - std::floor(at) < keep) kept.push_back(trees[i]);
  }
  trees = std::move(kept);
}

}  // namespace ge::mountain::mapgen
