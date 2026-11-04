// `shooters_edge_server`: Shooter's Edge's headless dedicated server, the engine's dedicated
// process (engine/server/main.hpp: server.cfg, the flags, the run loop, the status lines, the
// digest) over the game's playable ServerGame (net/classic_game.hpp): the mod's deathmatch served
// over the classic protocol, so a classic client connects and plays. `--help` lists the flags.
//
// ge:summary The headless dedicated server: the engine's process over Shooter's Edge's ServerGame
// ge:keywords dedicated-server shooters_edge_server headless main classic-protocol server-cfg
// ge:skill shooter net
// ge:test tests/shooters_edge_classic_game_test.cpp
#include "server/main.hpp"
#include "shooters_edge/net/classic_game.hpp"

int main(int argc, char** argv) {
  return ge::server::server_main(argc, argv, shooters_edge::net::classic_game());
}
