// A MAP'S PLACE NAMES IN THE WINDOW: the points a team calls places by (plugins/classic/locs.hpp),
// read when a map starts and made by the player in the game, standing in each place and naming it.
//
// - **Read** (`map_started`): when the map's name changes from the last one read for, the points
//   are forgotten and, while `tp_loadlocs` is 1 (the default), the map's file is read: the one the
//   player saved under the game folder on disk (`<game folder>/locs/<map>.loc`, any letter case)
//   first, else the game's own (the adapter's `open_locations`, archives too). The same map started
//   again keeps the points, so edits not yet saved survive a restart of the map. With neither, a
//   map in a map group reads the group's file the same way (`mapgroup`, below).
// - **Map and sky groups** (`mapgroup`, `skygroup`: classic/content/map_groups.hpp): the groups
//   the search path's lookups fall back through, registered here beside the place commands.
// - **Made** (the console, each with its older short name): `locations_add <name>` (`addloc`)
//   puts a point where the player stands (the middle of the player's box), `locations_remove`
//   (`removeloc`) takes away the point nearest the player, `locations_clearall` (`clearlocs`)
//   forgets every point, `locations_savefile <name>` (`saveloc`) writes them to
//   `<game folder>/locs/<name>.loc` (`.loc` added when the name has none; `saveloc $mapname`), and
//   `locations_loadfile <name>` (`loadloc`) reads a file in their place. `loc` says where the
//   player is: the nearest point's name, or `tp_name_someplace` ("someplace") with none.
// - **Quake II's spelling**: `loc add <name>` (the rest of the line), `loc del`, `loc set
//   <name>` (renames the nearest point in place), `loc list` and `loc save [name]` (the map's name
//   by default) do the same, so a player used to Quake II's client finds them.
// - **How far a place reaches** (`loc_dist`, the game's `place_range` by default: 500 units in
//   every classic game; 0 no limit): the nearest point within it names where the player is, and
//   is the one `removeloc`, `loc del` and `loc set` act on.
// - **Through walls** (`loc_trace`, 0 by default): with 1, a point whose straight line from the
//   player is blocked by the map's solid is passed over (`clear_line`), so a place behind a wall
//   is not the one the player is in.
// - **Where the player is**: the window's own game, a server joined, or a demo played (the view's
//   point less its eye height); the map's place names are read as each one's map starts.
// - **The place macros**, in every game (Quake II's client brought them): `$loc_here` is the place
// the player is
//   in and `$loc_there` the place nearest where the view's line stops (`looked_at`: 8,192 units
//   from where the player stands, or the first solid), each within `loc_dist`, `unknown` with
//   none or no game; every console line expands them (`echo I am at $loc_here`).
// - **Chat** (`fill_place_codes`): `%l` in a line the player says becomes the place they are in
//   before it is sent, so the others see the name, never `%l` (team_chat.hpp's codes ask it).
// - The file's text is the shared format's (`locs_text`): eighths of a unit, the names as typed,
//   their `$loc_name_<key>` words kept.
//
// The lines it prints are a player's: plain words, no next steps.
//
// ge:summary The window's place names: read at map start, added, removed and saved in the game
// ge:keywords place-names loc locations loc-editing locations_add locations_remove addloc
// ge:keywords removeloc locations_savefile saveloc locations_loadfile loadloc locations_clearall
// ge:keywords clearlocs tp_loadlocs loc_dist place-range tp_name_someplace map_started where-am-i
// ge:keywords loc-add loc-set loc-list loc-save loc_here loc_there place-macros
// ge:keywords fill_place_codes percent-l chat-location say say_team loc_trace clear_line
// ge:keywords mapgroup skygroup map-group
// ge:skill shooter console
// ge:test tests/shooters_edge_place_names_test.cpp
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "classic/locs.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

// Where the player stands (the middle of the player's box), false when no game is playing.
using WhereAmI = std::function<bool(ge::shooter::Vec3f&)>;

struct PlaceNames {
  ge::classic::Locs locs;
  std::string map;                               // the map the points were read for ("": none yet)
  std::filesystem::path game_dir;                // the game folder on disk a file is saved under
  const ge::console::Cvar* load = nullptr;       // tp_loadlocs (null: on)
  const ge::console::Cvar* someplace = nullptr;  // tp_name_someplace
  const ge::console::Cvar* range = nullptr;      // loc_dist (null: no limit)
  // Where the player's view stops: the line along it from where the player stands, 8,192 units
  // or to the first solid (`$loc_there`); false (or null) when no game is playing.
  std::function<bool(ge::shooter::Vec3f&)> looked_at;
  // Whether the straight line between two points is clear of the map's solid (`loc_trace` 1: a
  // place's point behind a wall is not where the player is); null: no map to ask.
  ge::classic::LocSight clear_line;
  const ge::console::Cvar* trace = nullptr;  // loc_trace
  const ge::console::Cvar* draw = nullptr;   // loc_draw (world_fx.hpp's place markers)
  WhereAmI where;  // where the player stands (add_place_commands keeps it)
  // Where a player can be in the map played or watched (its open space sampled), for `pruneloc`;
  // empty (or null) when no map is loaded.
  std::function<std::vector<ge::shooter::Vec3f>()> spots;
};

// The map `map` started: its points read as the header says (`adapter` and `files` the game's).
void map_started(PlaceNames& p, const GameAdapter& adapter, const SearchPath& files,
                 const std::string& map);

// The place commands and cvars above (`p`, `adapter`, `files` and `where` outlive `con`).
void add_place_commands(ge::console::Console& con, PlaceNames& p, const GameAdapter& adapter,
                        const SearchPath& files, const WhereAmI& where);

// `loc_draw`'s markers: every place point within `loc_dist` of where the player is (a whole
// number of units), drawn where `at` says, the nearest (by `loc_trace`'s sight) bobbing 10 units
// on a 0.63 s swing with its light at the point itself. False (and none) when `loc_draw` is off,
// no game is playing or no point is nearest.
struct PlaceMarker {
  ge::shooter::Vec3f at, light;
  bool nearest = false;
};
bool place_markers(const PlaceNames& p, double t, std::vector<PlaceMarker>& out);

// The place `at` is in, as `loc` names it (`tp_name_someplace` with none).
std::string place_at(const PlaceNames& p, ge::shooter::Vec3f at);

// A chat line with its place codes filled in before it is sent, so the others read the place:
// `%l` is the place the player is in (as `loc` says it: `tp_name_someplace` with none).
std::string fill_place_codes(const PlaceNames& p, std::string_view text);

}  // namespace shooters_edge::client
