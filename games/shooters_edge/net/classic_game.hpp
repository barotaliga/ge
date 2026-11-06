// SHOOTER'S EDGE'S PLAYABLE DEDICATED SERVER: the engine's server core (engine/server) running the
// game's own deathmatch (the baseline mod, compiled from the sources built into the game, as the
// listen server plays it) and serving it over the classic protocol (classic_server.hpp) on the
// game's port. The core keeps the frame clock, the settings (server.cfg in layers, the flags) and
// the process; the endpoint answers its own clients (`ServerGame::serve`), so a classic client
// connects to `shooters_edge_server` and plays.
//
// THE SETTINGS it reads: `sv_hostname` and `sv_password` (the server info and the connect's
// check), `sv_maxclients` (the loop's slots), `sv_map` (`arena`, built in, or `maps/<name>.bsp`
// from the game folder under `--root`, loose or in a pack), `sv_seed` (the mod's stream and the
// challenges'), and the mod's own rules (`fraglimit`, `timelimit`, `deathmatch`, `teamplay`, …:
// `ServerGame::rules`), each left to the mod's default when unset, and `sv_maprotation` (the map
// after each match's end; unset, the one the mod asks for).
//
// A MOD FOLDER (`sv_mod`, `--mod=<folder>`; the last of a comma list): the maps and files are its
// folder's over the base game's, the program is the folder's own multiplayer program (the game
// adapter's `find_program`: `qwprogs.dat`, loose or in any of its packs) in place of the baseline
// mod, serverdata names the folder, and the server info's `*gamedir` says it before the map loads
// (a mod checks it). A mod folder without a program this server runs is refused, naming it.
//
// BOTS AND THE LOBBY: `sv_bots` seats the listen server's fighters (client/lobby_room.hpp) in the
// slots after 0, each a local seat of the endpoint every client sees as a player. `sv_lobby 1`
// runs the engine's lobby (engine/server/match.hpp): every map warms up with the limits held, a
// client's `ready`, `break`, `team`, `start` and `lobby` (and the leader's `addbot`, `removebot`)
// are taken before the mod, a seat's team goes into its user info and the mod's `.team`, the
// countdown run out restarts the map into the match, and the match's next map warms up again.
// Bots alone never count down. Without `sv_lobby` the bots play every map as a match.
//
// Offline (no socket: a test, a lab) the frame is the mod's own frame with the bots' moves; on
// the network it is the endpoint's frame at the server's time, which is the same frame while
// nobody is connected, so both give one digest.
//
// ge:summary Shooter's Edge's playable dedicated server: the mod on the classic protocol
// ge:keywords dedicated-server classic-server servergame serve net_frame baseline-mod deathmatch
// ge:keywords classic_game playable-server game-rules fraglimit timelimit map-change maprotation
// ge:keywords sv_bots sv_lobby bots lobby warmup addbot removebot
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#pragma once

#include "server/server.hpp"

namespace shooters_edge::net {

// The game folder under `--root` its maps are read from (the client's own default).
inline constexpr const char* kGameFolder = "id1";

const ge::server::ServerGame& classic_game();

}  // namespace shooters_edge::net
