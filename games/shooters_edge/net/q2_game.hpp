// QUAKE II'S DEDICATED SERVER GAME: the engine's server core (engine/server) running Quake II's
// rules (server/q2_server.hpp) on a Quake II map and serving them over Quake II's own protocol
// (q2_net_server.hpp, protocol 34) on the game's port, so an existing client of Quake II
// connects to `sesv +set fs_basegame q2` and plays.
//
// THE MAP is `sv_map` (default `q2dm1`) read from Quake II's game folder under `--root`
// (`baseq2`, loose or in its packs: the content row `q2`, classicenhance/profile.hpp). The
// rules run deathmatch; `sv_hostname`, `sv_password` and `sv_maxclients` are the endpoint's. The
// map's checksum and sky go in its configstrings, as a client checks its own copy of the map.
// THE LEVEL CHANGES when the rules ask (an intermission's end after the frag or time limit, an
// exit, `map`, `changelevel`, `restart`): the map is read, the rules start on it, and every client
// is told and signs on again on its connection. The rules' `sv_maplist` is `sv_map` then
// `sv_maprotation`, wrapping; a map that cannot be read starts this one again.
//
// THE ADMINISTRATOR (q2_admin.hpp): `rcon_password` opens the remote console; the ban list is
// kept in the game folder's `listip.cfg` and read back at start; `sv_banmode` (Quake II's own
// spelling, `filterban`, is its synonym here) says how it is read.
//
// LOGINS: with `sv_login` on, or accounts handed in (its own master, or `sv_accountmaster`), the
// players log in as on Quake's server (q2_login.hpp).
//
// Offline (no socket) the frame is the rules' own with nobody's moves.
//
// ge:summary Quake II's dedicated server game: its rules served over protocol 34 by the core
// ge:keywords q2-dedicated-server sesv protocol-34 servergame baseq2
// ge:keywords q2_game q2-login level-change sv_maprotation sv_maplist rcon filterban
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#pragma once

#include "server/server.hpp"

namespace shooters_edge::net {

const ge::server::ServerGame& q2_game();

}  // namespace shooters_edge::net
