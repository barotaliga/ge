// A BOT'S ROUTE ON THE LIVE MAP: a campaign run's track drawn over the live map's plate
// (live_map.hpp `build_plate`: the map's own floors from above, its liquids, the blueprint round
// them), so a run reads as the map a viewer of TV knows. The track a line in time's colours (the
// start blue, the end red, so a part walked twice shows which came first), the start a square, each
// monster's fall a dark cross, each secret found a gold diamond, each exit a ring. Any campaign
// runner's lab draws its runs with it (Quake's `se-q1-campaign`, Quake II's unit).
//
// ge:summary A bot's route drawn on the live map's plate: the track, kills, secrets and exits
// ge:keywords route route-picture live-map plate track kills secrets exits campaign play-through
// ge:keywords draw_route bot-route
// ge:skill map-design bots labs
// ge:test tests/shooters_edge_live_map_test.cpp
#pragma once

#include <array>
#include <vector>

#include "shooters_edge/livemap/live_map.hpp"
#include "ui/image.hpp"

namespace shooters_edge::livemap {

// What a run left on a map, in level units (x, y, z).
struct RouteMarks {
  std::vector<std::array<float, 3>> track;    // a point every half second, first to last
  std::vector<std::array<float, 3>> kills;    // where each monster fell
  std::vector<std::array<float, 3>> secrets;  // where he stood as each secret counted
  std::vector<std::array<float, 3>> exits;    // each exit's middle
};

inline constexpr float kRouteJump = 400;  // a step this long is a teleporter: not drawn, units

// The plate's picture with the route drawn over it.
ge::ui::Image draw_route(const Plate& plate, const RouteMarks& m);

}  // namespace shooters_edge::livemap
