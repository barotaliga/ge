// SHOOTER'S EDGE'S PLAYABLE DEDICATED SERVER: the engine's server core (engine/server) running the
// game's own deathmatch (the baseline mod, compiled from the sources built into the game, as the
// listen server plays it) and serving it over the classic protocol (classic_server.hpp) on the
// game's port. The core keeps the frame clock, the settings (server.cfg in layers, the flags) and
// the process; the endpoint answers its own clients (`ServerGame::serve`), so a classic client
// connects to `shooters_edge_server` and plays.
//
// THE SETTINGS it reads: `sv_hostname` and `sv_password` (the server info and the connect's
// check), `sv_maxclients` (the loop's slots), `sv_map` (`arena`, built in, or `maps/<name>.bsp`
// from the game folder under `--root`), `sv_seed` (the mod's stream and the challenges').
// `sv_bots` must be 0: bots on the classic server come with the engine's lobby (M53c).
//
// Offline (no socket: a test, a lab) the frame is the mod's own frame with no moves; on the
// network it is the endpoint's frame at the server's time, which is the same frame while nobody
// is connected, so both give one digest.
//
// ge:summary Shooter's Edge's playable dedicated server: the mod on the classic protocol
// ge:keywords dedicated-server classic-server servergame serve net_frame baseline-mod deathmatch
// ge:keywords classic_game playable-server
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#pragma once

#include "server/server.hpp"

namespace shooters_edge::net {

// The game folder under `--root` its maps are read from (the client's own default).
inline constexpr const char* kGameFolder = "id1";

const ge::server::ServerGame& classic_game();

}  // namespace shooters_edge::net
