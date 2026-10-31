// SHOOTER'S EDGE ON THE ENGINE'S SERVER CORE: the game's `ServerGame` (engine/server/server.hpp),
// which `shooters_edge_server` (server/bin/main.cpp) hands to the engine's dedicated process and a
// listen server will step from the client (M53a). It runs the server's host (host.hpp) on the
// config's map with its bot slots filled by stand-in wanderers (stand_in.hpp) until the real bots
// take them (M18r), keeps the input log when one is asked for, and replays one.
//
// The run is the host's own, frame for frame: the digest of a run through this table is the
// digest the host gives when driven by hand (tests/server_core_test.cpp holds both).
//
// ge:summary Shooter's Edge's ServerGame: the host, stand-in bots, the input log, the replay
// ge:keywords dedicated-server servergame server-core shooters_edge_server stand-in bots
// ge:keywords input-log replay dedicated_game
// ge:skill shooter net
// ge:test tests/server_core_test.cpp
#pragma once

#include "server/server.hpp"

namespace shooters_edge::server {

// The UDP port and the LAN discovery port Shooter's Edge's server takes by default.
constexpr uint16_t kServerPort = 27500;
constexpr uint16_t kDiscoveryPort = 27510;

const ge::server::ServerGame& dedicated_game();

}  // namespace shooters_edge::server
