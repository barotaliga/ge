// A MAP'S PLACE NAMES IN THE WINDOW: the points a team calls places by (plugins/shooter/locs.hpp),
// read when a map starts and made by the player in the game, standing in each place and naming it.
//
// - **Read** (`map_started`): when the map's name changes from the last one read for, the points
//   are forgotten and, while `tp_loadlocs` is 1 (the default), the map's file is read: the one the
//   player saved under the game folder on disk (`<game folder>/locs/<map>.loc`) first, else the
//   game's own (the adapter's `open_locations`, archives too). The same map started again keeps the
//   points, so edits not yet saved survive a restart of the map.
// - **Made** (the console, each with its older short name): `locations_add <name>` (`addloc`) puts
// a
//   point where the player stands (the middle of the player's box), `locations_remove`
//   (`removeloc`) takes away the point nearest the player, `locations_clearall` (`clearlocs`)
//   forgets every point, `locations_savefile <name>` (`saveloc`) writes them to
//   `<game folder>/locs/<name>.loc` (`.loc` added when the name has none; `saveloc $mapname`), and
//   `locations_loadfile <name>` (`loadloc`) reads a file in their place. `loc` says where the
//   player is: the nearest point's name, or `tp_name_someplace` ("someplace") with none.
// - The file's text is the shared format's (`locs_text`): eighths of a unit, the names as typed,
//   their `$loc_name_<key>` words kept.
//
// The lines it prints are a player's: plain words, no next steps.
//
// ge:summary The window's place names: read at map start, added, removed and saved in the game
// ge:keywords place-names loc locations loc-editing locations_add locations_remove addloc
// ge:keywords removeloc locations_savefile saveloc locations_loadfile loadloc locations_clearall
// ge:keywords clearlocs tp_loadlocs tp_name_someplace map_started where-am-i
// ge:skill shooter console
// ge:test tests/shooters_edge_place_names_test.cpp
#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "console/console.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

struct PlaceNames {
  ge::shooter::Locs locs;
  std::string map;                               // the map the points were read for ("": none yet)
  std::filesystem::path game_dir;                // the game folder on disk a file is saved under
  const ge::console::Cvar* load = nullptr;       // tp_loadlocs (null: on)
  const ge::console::Cvar* someplace = nullptr;  // tp_name_someplace
};

// Where the player stands (the middle of the player's box), false when no game is playing.
using WhereAmI = std::function<bool(ge::shooter::Vec3f&)>;

// The map `map` started: its points read as the header says (`adapter` and `files` the game's).
void map_started(PlaceNames& p, const GameAdapter& adapter, const SearchPath& files,
                 const std::string& map);

// The place commands and cvars above (`p`, `adapter`, `files` and `where` outlive `con`).
void add_place_commands(ge::console::Console& con, PlaceNames& p, const GameAdapter& adapter,
                        const SearchPath& files, const WhereAmI& where);

}  // namespace shooters_edge::client
