// A POINT TREE'S CELLS AS BRUSHES: a map that stores no brushes (version 29 and BSP2) given them,
// so the brush trace (trace.hpp) sweeps a box through it as through a version 38 map. Classic
// remix plays the classic game's maps on the sequel's rules this way (docs/shooters-edge.md §
// Classic remix).
//
// - **A cell** is the region a path from a model's head node reaches a leaf through: the planes
//   on the path, each faced so the region lies behind it. Every non-empty cell (solid, sky, water,
//   slime, lava) becomes one brush, and the node's child is pointed at a new leaf holding that
//   brush alone (a copy of the leaf the path reached: its cluster, its marked surfaces). A family
//   that sends all its solid space to one shared leaf so gets one leaf a cell.
// - **The faces**: each plane's polygon, cut by the cell's other planes and by the model's box
//   grown by `kCellMargin`, in doubles. A plane whose polygon is cut away bounds nothing and is
//   left out; the faces' corners are the cell's box.
// - **The bevels**: an axial plane at each side of that box the cell has no plane on, so a box
//   swept past an edge stops where it touches, not where the edge's planes cross.
// - **Contents** in the version 38 bits: solid 0x1, lava 0x8, slime 0x10, water 0x20; a sky cell
//   is solid and its sides carry the sky surface flag 0x4. Every leaf's contents become the same
//   bits (0 for empty), as the trace's mask reads them.
//
// Each step is a pure function of the map, in a fixed order, so the brushes are the same bits on
// every machine (tests/brush_cells_test.cpp).
//
// ge:summary A version 29 map's point-tree cells made brushes, so the brush trace sweeps it
// ge:keywords cells brushes point-tree version-29 bsp2 bevel brush-trace classic-remix
// ge:keywords solid-cell leaf-brushes contents-bits sky-flag
// ge:skill brush
// ge:test tests/brush_cells_test.cpp
#pragma once

#include <cstdint>

#include "brush/map.hpp"

namespace ge::brush {

// How far beyond a model's box an open cell (the void around the world) is closed.
inline constexpr double kCellMargin = 64;
// The version 38 bits a cell's brush carries.
inline constexpr int32_t kCellSolid = 0x1, kCellLava = 0x8, kCellSlime = 0x10, kCellWater = 0x20;
inline constexpr int32_t kCellSkyFlag = 0x4;

struct CellCount {
  uint32_t brushes = 0;  // cells made brushes
  uint32_t sides = 0;    // their sides, the bevels with them
  uint32_t bevels = 0;   // axial sides added
  uint32_t dropped = 0;  // planes on a path that bound nothing
};

// `map` given a brush for every non-empty cell of every model's tree (above). A map that has
// brushes already is left as it is.
CellCount cells_as_brushes(Map& map);

}  // namespace ge::brush
