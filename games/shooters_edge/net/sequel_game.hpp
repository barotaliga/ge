// THE SEQUEL'S DEDICATED SERVER GAME: the engine's server core (engine/server) running the sequel's
// rules (server/sequel_server.hpp) on a sequel map and serving them over the sequel's own protocol
// (sequel_net_server.hpp, protocol 34) on the game's port, so an existing client of the sequel
// connects to `shooters_edge_sequel_server` and plays.
//
// THE MAP is `sv_map` (default `q2dm1`) read from the sequel's game folder under `--root`
// (`baseq2`, loose or in its packs: the content row `sequel`, classicenhance/profile.hpp). The
// rules run deathmatch; `sv_hostname`, `sv_password` and `sv_maxclients` are the endpoint's. The
// map's checksum and sky go in its configstrings, as a client checks its own copy of the map.
// THE LEVEL CHANGES when the rules ask (an intermission's end after the frag or time limit, an
// exit, `map`, `changelevel`, `restart`): the map is read, the rules start on it, and every client
// is told and signs on again on its connection. The rules' `sv_maplist` is `sv_map` then
// `sv_maprotation`, wrapping; a map that cannot be read starts this one again.
//
// Offline (no socket) the frame is the rules' own with nobody's moves.
//
// ge:summary The sequel's dedicated server game: its rules served over protocol 34 by the core
// ge:keywords sequel-dedicated-server shooters_edge_sequel_server protocol-34 servergame baseq2
// ge:keywords sequel_game level-change sv_maprotation sv_maplist
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include "server/server.hpp"

namespace shooters_edge::net {

const ge::server::ServerGame& sequel_game();

}  // namespace shooters_edge::net
