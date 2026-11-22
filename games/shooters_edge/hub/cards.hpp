// THE HUB'S MATCH CARDS, THE GAME'S HALF (docs/hub.md § Match cards): what Shooter's Edge's hub
// gives the engine's card (engine/hub/card.hpp) and its stills (engine/hub/stills.hpp) to draw a
// classic game's match the way the game shows it:
//
// - **the game** of a record: the server's `*game` key (net/qw_game.cpp: `q1` or `q2`), else
//   Quake's (a QuakeWorld server found by the browse);
// - **the content**: the hub's `--content` folder holds each game's install under its id, laid out
//   as the game's own folder is (`<content>/q1/id1/pak0.pak`, `<content>/q2/baseq2/pak0.pak`),
//   opened through the game's own search path (client/files.hpp);
// - **the board's look**: the game's own glyph sheet (`conchars`, from the packs, never enhanced)
//   and its board: Quake's frags on a box of the player's colour with the words in the plain set,
//   Quake II's frags bare with the words in its second (green) set;
// - **the still's camera** (`still_view`): the map's intermission spot as the game's rules pick it
//   (Quake's `info_intermission` by its `mangle`, Quake II's `info_player_intermission` by its
//   `angles`; the first one, so the still is the same every time), else the first deathmatch
//   spawn, else the player's start, at the eye's height over it, looking the way it faces.
//
// ge:summary Shooter's Edge's half of the hub's match cards: the game, its content, charset, camera
// ge:keywords hub match-card still intermission info_intermission info_player_intermission mangle
// ge:keywords conchars charset card-look game-of content hub_game_of still_view hub_card_look
// ge:skill net shooter
// ge:test tests/shooters_edge_hub_cards_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "brush/map.hpp"
#include "hub/card.hpp"
#include "net/status.hpp"
#include "shooters_edge/client/files.hpp"

namespace shooters_edge::hub {

using client::SearchPath;

// The classic game a record's match is of ("q1", "q2").
std::string hub_game_of(const ge::net::Status& s);

// `game`'s install under `content` opened into `out`. False with the reason.
bool open_hub_content(std::string_view content, std::string_view game, SearchPath& out,
                      std::string& error);

// The board's look for `game` from its packs under `content`. False with the reason.
bool hub_card_look(std::string_view content, std::string_view game, ge::hub::CardLook& out,
                   std::string& error);

// Where a map's still is taken from, in the map's own frame (units, z up; degrees, pitch down
// positive), and which entity gave it.
struct StillView {
  float origin[3] = {0, 0, 0};
  float pitch = 0, yaw = 0;
  std::string from;  // "info_intermission", "info_player_deathmatch", ... ("" none: the centre)
};
StillView still_view(const ge::brush::Map& map);

}  // namespace shooters_edge::hub
