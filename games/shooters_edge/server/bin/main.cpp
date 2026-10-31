// `shooters_edge_server`: Shooter's Edge's headless dedicated server, the engine's dedicated
// process (engine/server/main.hpp: server.cfg, the flags, the run loop, the status lines, the
// digest, the input log and its replay) over the game's ServerGame (server/dedicated.hpp). Until
// the protocols land (M18g, M18h) no client joins over the network, so its slots are bots: the
// stand-in wanderer until Shooter's Edge's bots land (M18r). `--help` lists the flags.
//
// ge:summary The headless dedicated server: the engine's process over Shooter's Edge's ServerGame
// ge:keywords dedicated-server shooters_edge_server headless main bots server-cfg
// ge:skill shooter net
// ge:test tests/server_core_test.cpp
#include "server/main.hpp"
#include "shooters_edge/server/dedicated.hpp"

int main(int argc, char** argv) {
  return ge::server::server_main(argc, argv, shooters_edge::server::dedicated_game());
}
