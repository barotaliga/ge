// A MAP SURVEYED FOR A LAB: what each place of a map is, told the same way in every family, so a
// lab or an agent places an eye, a view or a run without probing the map by hand. The brush tool
// (tool.hpp) prints each as JSON.
//
// - **Cells**: a leaf's contents as one word, `empty`, `solid`, `water`, `slime`, `lava` or `sky`.
//   29 and BSP2 read their codes (−2 solid, −3 water, −4 slime, −5 lava, −6 sky; a current, −9 and
//   below, is water; any other code is empty); 38 its bits (solid or window is solid, then lava,
//   slime, water); 46 stores none, so a leaf is solid or empty.
// - **Liquid bodies**: the world's water, slime and lava leaves, each kind's leaves joined where
//   their boxes touch or overlap, one body a pool. A body's box is its leaves' boxes together and
//   its depth the box's height, so a leaf box that reaches above the surface reads deeper than the
//   pool is. Deepest first. A body's `narrowest` is its box's smaller side across (x or y): a
//   slab of liquid a few units thick (a window, a gate's face) reads deep and narrow.
// - **A slice**: the world's leaf cells sampled on a plane across the world model's box, one
//   character a cell at the cell's centre (`cell_char`). On `--x` the columns run along y and the
//   rows down z; on `--y` the columns along x, rows down z; on `--z` the columns along x and the
//   rows down y (north at the top).
// - **An entity's place**: a brush entity (`model` `*<n>`) is its model's box moved by its
//   `origin`; any other entity is its `origin`. An entity with neither has no place.
//
// ge:summary A map surveyed: leaf cells by family, liquid bodies, a slice, entities near a point
// ge:keywords survey cell contents slice cross-section liquid liquids pool deepest depth near
// ge:keywords radius entity-place brush-bounds origin glob class-glob
// ge:skill brush agent-interface
// ge:test tests/brush_survey_test.cpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"

namespace ge::brush {

enum class Cell : uint8_t { empty, solid, water, slime, lava, sky };

Cell leaf_cell(const Map& map, const Leaf& leaf);
const char* cell_name(Cell c);
// `.` empty, `#` solid, `~` water, `%` slime, `!` lava, `^` sky.
char cell_char(Cell c);

struct LiquidBody {
  Cell kind = Cell::water;
  float mins[3] = {}, maxs[3] = {};
  uint32_t leaves = 0;
  float depth() const { return maxs[2] - mins[2]; }
  float narrowest() const { return std::min(maxs[0] - mins[0], maxs[1] - mins[1]); }
};

// The world's liquid bodies, deepest first (then by kind and the box's corner).
std::vector<LiquidBody> liquid_bodies(const Map& map);

struct Slice {
  int axis = 0;                   // 0 x, 1 y, 2 z: the axis the plane is across
  double at = 0, step = 0;        // the plane's coordinate, a cell's size (map units)
  double u0 = 0, v0 = 0;          // the first column's and the first row's centre
  int u_axis = 1, v_axis = 2;     // the axes the columns and the rows run along
  std::vector<std::string> rows;  // top row first; the v coordinate falls row by row
};

// The world's cells on the plane `axis` = `at`, `step` units a cell, over the world model's box.
// `step` <= 0 picks one that keeps the longer side to about 96 cells. False with the reason when
// the plane misses the box or the grid would pass `max_cells` a side.
bool slice_map(const Map& map, int axis, double at, double step, Slice* out, std::string* error,
               int max_cells = 400);

// Where entity `e` is: its box (a point entity's is its origin twice). False when it has none.
bool entity_place(const Map& map, const Entity& e, float mins[3], float maxs[3]);
// The distance from `p` to the box (0 inside it).
double box_distance(const float mins[3], const float maxs[3], const float p[3]);

// Whether `name` matches `glob` (`*` any run, `?` any one character; case kept).
bool glob_match(std::string_view glob, std::string_view name);

}  // namespace ge::brush
