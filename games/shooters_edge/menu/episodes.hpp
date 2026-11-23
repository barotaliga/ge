// QUAKE'S EPISODES AND LEVELS: the lists the game's own new-game page picks a map from, by episode
// then level, each with its long name as the page shows it. The base game's seven (the start map,
// the four episodes, the final level and the deathmatch arenas) and each mission pack's own; which
// list applies, and which of its episodes, follows the maps the packs hold (`episodes_for`): the
// shareware packs show the first two, a mission pack's maps its own list.
//
// The host page (main_menu.cpp) shows EPISODE and LEVEL over its MAP row and keeps the three
// together: a new episode starts at its first level, a level picked sets the map, a map typed or
// picked that a list holds sets its episode and level.
//
// ge:summary Quake's episode and level lists with their long names, and which apply to the packs
// ge:keywords episode level episodes levels new-game host-page mission-pack long-name map-list
// ge:keywords episodes_for level_of
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::menu {

struct Level {
  std::string_view map;   // the map file's name
  std::string_view name;  // its long name
};

struct Episode {
  std::string_view name;
  std::span<const Level> levels;
};

// The list that fits `maps` (the names the packs hold): a mission pack's when its first episode's
// map is there, else the base game's; only its episodes whose first level is there. Empty when no
// list fits (another game's maps).
std::vector<Episode> episodes_for(std::span<const std::string> maps);

// Where `map` stands in `episodes`: its episode and level; false when no level is it.
bool level_of(std::span<const Episode> episodes, std::string_view map, int* episode, int* level);

}  // namespace shooters_edge::menu
