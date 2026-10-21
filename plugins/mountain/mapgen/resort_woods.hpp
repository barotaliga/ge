// R14 ON A SKI AREA — THE WOODS BY HEIGHT: thick low on the mountain, thinning up through the
// ecotone to the tree line, bare above it, and kept off the runs, the lifts and the hub.
//
// A real mountain is not wooded at one density from the valley floor up. Forest cover falls with
// height on an S-curve: CLOSED forest (crowns touching, few glades) up to the TIMBERLINE, then an
// ECOTONE where the woods break into patches, groups and lone trees, to the TREE LINE, above
// which nothing stands. The ecotone is a hundred to a hundred and fifty metres of height in the
// high ranges, thinner on a northern fell where the birch ends abruptly, wider in a heavy-snow
// range whose upper woods are parkland. Trees shrink the same way: the tallest low down, a third
// of that at the timberline, stunted at the line.
//
// So a spot's woods are kept at `logistic(d)` of the rule's density, where `d` is how far under
// the tree line it stands (half at the ecotone's midpoint, near none at the line), and below the
// timberline the glades close: the slow noise's woods are pulled toward the thick of a wood, so
// the lower mountain reads as forest with runs cut through it and the upper as islands of trees
// on open snow. The numbers are a region's row.
//
// ge:summary R14 a ski area's woods by height: the ecotone's curve and the trees' heights
// ge:keywords resort-woods woods-by-height ecotone timberline tree-line
// ge:keywords woods-at-depth tall-at-depth
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include "mountain/mapgen/forest.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// One region's woods by height (R14, R21): the ecotone's midpoint and steepness under the tree
// line, how far under the line the glades close, and the height curve's reach, all m.
struct WoodsRow {
  double mid;
  double steep;
  double closed;
  double reach;
};

// The region's row.
const WoodsRow& woods_row(RegionId id);

// R14 — how much of the woods stand `d` m under the tree line, and how far toward the thick of a
// wood the glades close there.
WoodsCover woods_at_depth(const WoodsRow& row, double d);

// R14 — the tallest a tree `d` m under the tree line grows, as a share of the height band: the
// krummholz's at the line, the whole band by the curve's reach and below.
double tall_at_depth(const WoodsRow& row, double d);

}  // namespace ge::mountain::mapgen
