// A SKY BOX TYPED FOR ONE MAP (`sky <name> [rotate] [axis x y z]`, the window's word): the box's
// name, the map it was typed on (a map of another name is its own sky again), its turn in degrees
// a second about an axis in the map's own settings' units (0 0 1 when none is given), and a count
// of the times it was typed (0: never). The look keeps one (app/look.hpp's `sky_typed`); the world
// reads it when it is made.
//
// ge:summary A sky box typed for one map: its name, the map, its turn and axis, a count
// ge:keywords sky sky_typed skyrotate skyaxis sky-box
// ge:skill classic render
// ge:test tests/shooters_edge_render_words_test.cpp
#pragma once

#include <array>
#include <string>
#include <string_view>

namespace ge::classic {

struct SkyTyped {
  std::string name, map;
  double rotate = 0;
  std::array<double, 3> axis{0, 0, 1};
  int serial = 0;
  // Whether it stands for the map named `on`.
  bool for_map(std::string_view on) const { return serial > 0 && map == on; }
};

}  // namespace ge::classic
