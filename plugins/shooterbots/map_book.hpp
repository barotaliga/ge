// A GAME'S BOOK OF ITS MAPS: what people do on a game's own maps, read off their recorded games, as
// tables the game fills by map name (Shooter's Edge's: games/shooters_edge/bots/quake_maps.hpp).
// The bots' nav proves each row by the move when a map's nav is built, so a row the move cannot
// fly is left out:
// - ROCKET-JUMP TRICKS (`RocketTrick`): a takeoff and a landing, the way it is flown (the flip or
//   the shot at the feet), its pitch and run and the least jumper's level that makes it (0
//   amateur, 1 intermediate, 2 expert: the lowest whose players made a fifth of its jumps or
//   more); proven into rocket links (nav.hpp `nav_add_tricks`).
// - GAP TRICKS (`GapTrick`): the plain jumps people make over a pit at a hopper's speed, the
//   takeoff's speed (its quarter), whether the pit holds lava or slime, the level; proven into
//   jump links (nav_gaps.hpp `nav_add_gap_tricks`).
// - SCRIPTED ROUTES (`ItemRoute`): the last stretch to a key item written as a person takes it,
//   where the proven graph has no such way: a run of points (a player's feet), the first its
//   entry (joined to the nav's nearest node), each after it walked or hopped onto, the last the
//   item's own node; proven as one `script` link (nav_script.hpp `prove_script`).
// - ITEM RULES (`ItemRule`): an item people never go for while another is to be had first; it is
//   no goal while the item of call `after` is believed up or due within `due_s`.
// A map's name is its file's without folder or ".bsp" (`map_base`). A book with no table for a
// kind (a null call) has none of it on any map.
//
// ge:summary A game's book of its maps: rocket and gap tricks, scripted routes, item rules by map
// ge:keywords map-book rocket-tricks gap-tricks item-routes item-rules scripted-route map_base
// ge:keywords nav_tricks nav_gap_tricks map-data copy-movement movement-skill
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_gaps_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "shooterbots/kit.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/nav_gaps.hpp"

namespace ge::shooterbots {

struct RocketTrick {
  const char* map;
  Vec3f from, to;  // people's takeoff and landing (origins)
  float pitch;     // the shot, degrees down
  float run;       // the run at the shot, units/s (0: standing)
  int jumps;       // how often people made it in the games read
  bool flip;       // flown as the flip (the shot behind)
  uint8_t level;   // 0 amateur, 1 intermediate, 2 expert
};

struct GapTrick {
  const char* map;
  Vec3f from, to;  // people's takeoff and landing (origins)
  float speed;     // the takeoff's speed across, its quarter, units/s
  int jumps;       // how often people made it in the games read
  bool harm;       // the pit holds lava or slime
  uint8_t level;   // 0 amateur, 1 intermediate, 2 expert
};

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

// The item of call `item` within 48 units of (x, y, z), no goal while the item of call `after` is
// believed up or due within `due_s` (the fighter's `best_item`, under the `routes` switch).
struct ItemRule {
  const char* item;
  float x, y, z;
  const char* after;
  float due_s;
};

struct MapBook {
  std::span<const RocketTrick> (*rocket_tricks)(std::string_view map) = nullptr;
  std::span<const GapTrick> (*gap_tricks)(std::string_view map) = nullptr;
  std::span<const ItemRoute> (*item_routes)(std::string_view map) = nullptr;
  std::span<const ItemRule> (*item_rules)(std::string_view map) = nullptr;
};

// The book's rows for map `map` (its base name); none where the book has no such table.
std::span<const RocketTrick> rocket_tricks(const MapBook& book, std::string_view map);
std::span<const GapTrick> gap_tricks(const MapBook& book, std::string_view map);
std::span<const ItemRoute> item_routes(const MapBook& book, std::string_view map);
std::span<const ItemRule> item_rules(const MapBook& book, std::string_view map);

// The rows as the nav's.
std::vector<NavTrick> nav_tricks(std::span<const RocketTrick> tricks);
std::vector<NavGapTrick> nav_gap_tricks(std::span<const GapTrick> tricks);

// A map's name as the tables key it: "maps/e1m2.bsp" -> "e1m2".
std::string_view map_base(std::string_view map);

}  // namespace ge::shooterbots
