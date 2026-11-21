// WHERE THE PLAYER IS FOR THE PLACE NAMES: the view of a server joined or a demo played (its eye
// less its height, its angles and its map), else the window's own game's player. The place
// names' three questions (where the player stands, where the view looks, whether a line between
// two points is clear) are answered off it, in the map watched or played (map_line.hpp).
//
// ge:summary The player's place, look and map the place names ask about, watched or played
// ge:keywords place-seen watched loc_trace looked-at clear-line where-am-i place-names
// ge:skill shooter
// ge:test tests/shooters_edge_place_names_test.cpp
#pragma once

#include <cstddef>

#include "console/console.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/place_names.hpp"
#include "shooters_edge/server/bsp_map.hpp"

namespace shooters_edge::client {

struct Listen;

// The view watched (a server joined, a demo), when `on`; else the window's own game is asked.
struct PlaceSeen {
  bool on = false;
  ge::shooter::Vec3f at{0, 0, 0};
  ge::shooter::Vec3f angles{0, 0, 0};   // the view's, degrees
  const server::BspMap* map = nullptr;  // its map's world (null: none read)
  int kept_level = -1;                  // the recording's level whose kept place names are in use
  size_t kept_count = 0;                // how many of them
};

// Where the view looks: 8,192 units along it from where the player stands, or the first solid.
inline constexpr float kLookReach = 8192;

// The place names' questions answered off `seen` (else `listen`'s player), and their commands
// (place_names.hpp's add_place_commands). All must outlive `con`.
void add_place_queries(ge::console::Console& con, PlaceNames& places, const Listen& listen,
                       const PlaceSeen& seen, const GameAdapter& adapter, const SearchPath& files);

}  // namespace shooters_edge::client
