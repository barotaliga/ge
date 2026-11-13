// THE FILES A MATCH ON A MAP DRAWS FROM (docs/hub.md § Playing in a browser): what the hub's
// content pack for playing a map in the page holds (engine/hub/content.hpp's `map_content`,
// `HubSpec::map_files`). The match is started headless on the map with bots, the same as the
// page starts it (the lobby, the bots ready, the countdown, the match), and run for
// `kPlayContentS` of game time; then every model and sound the server's lists hold is named, with
// the map (and its coloured light), what any picture of the game needs (demo_content.hpp's
// basics), the status bar's and the menus' pictures (`gfx.wad`, `gfx/`): the page's HUD, its
// scores and its lobby. A model or sound a mod precaches only later (a powerup's first pick-up) is
// missed and drawn without; the baseline mod precaches its weapons and items with the map.
//
// ge:summary The content paths a match on a map needs: its map, the server's models and sounds
// ge:keywords play content files map hub content-pack play_files map_files precache bots
// ge:skill shooter net
// ge:test tests/shooters_edge_play_content_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

inline constexpr double kPlayContentS = 4;  // game time the match runs before its lists are read
inline constexpr int kPlayContentBots = 3;

// Whether a map the packs hold is one to play: the game keeps its ammunition boxes and the like as
// maps (`b_*`), which are not.
bool map_playable(std::string_view map);

// The names a match on `map` (read from `files`) needs, lower case and sorted; `opt` the listen
// server's options (net/classic_game.hpp's `listen_options()`: the match's server). False with
// the reason when the map does not load or the match does not start.
bool play_files(const SearchPath& files, std::string_view map, const ListenOptions& opt,
                std::vector<std::string>& names, std::string& error);

}  // namespace shooters_edge::client
