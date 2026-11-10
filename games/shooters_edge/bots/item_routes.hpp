// SCRIPTED ROUTES TO KEY ITEMS, per map: the last stretch to an item written as a person takes
// it, where the proven graph has no such way (the yellow armour's stairs on e1m2 hopped up in one
// flow; the grid misses a 16-unit tread and the nav's standing jumps cost a second a step). Each
// route is a run of points (a player's feet), the first its ENTRY (joined to the nav's nearest
// node), each after it walked or hopped onto; the last is the item's own node. The nav proves
// each one with the move (shooter/nav_script.hpp `prove_script`) and adds it as one `script` link
// (bots/map_nav.cpp), so a route to the item leads to the entry and the script finishes it. A
// door over the way (e1m2's YA stairs, shut until a button is shot) bars the link while shut, and
// the fighter's button logic opens it as for any item behind a door.
//
// Read off the `se-spot` lab's pictures and se-reach's `--floors` (the treads' heights and
// depths), and people's own way up (se-rj `--spot`). A route that does not hold is tabled by
// se-reach as refused, never added.
//
// ge:summary Scripted routes to key items per map: the last steps walked and hopped, proven
// ge:keywords item-routes scripted-route item-route nav-script ya-stairs flowing-hop e1m2
// ge:keywords item-rules route-rules rockets-before-quad map_base
// ge:skill bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace shooters_edge::bots {

struct ItemRouteStep {
  float x = 0, y = 0, z = 0;  // a player's feet
  bool hop = false;           // jumped onto, else walked
};

inline constexpr size_t kItemRouteSteps = 8;

struct ItemRoute {
  const char* item;  // the item's call ("ya")
  const char* name;  // the way's own name ("ya-south")
  ItemRouteStep steps[kItemRouteSteps];
  size_t n;
};

// The map's scripted routes (the map's file name without its folder or ".bsp"); none for most.
std::span<const ItemRoute> item_routes(std::string_view map);

// AN ITEM RULE: an item people never go for while another is to be had first (e1m2: the rockets
// over the SNG pit never before the quad). The item is the one of call `item` within 48 units of
// (x, y, z); it is no goal while the item of call `after` is believed up or due within
// `due_s` (the fighter's `best_item`, under the `routes` switch).
struct ItemRule {
  const char* item;
  float x, y, z;
  const char* after;
  float due_s;
};
std::span<const ItemRule> item_rules(std::string_view map);

// A map's name as the tables key it: "maps/e1m2.bsp" -> "e1m2".
std::string_view map_base(std::string_view map);

}  // namespace shooters_edge::bots
