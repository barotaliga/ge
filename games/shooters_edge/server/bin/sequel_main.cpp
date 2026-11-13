// `shooters_edge_sequel_server`: Shooter's Edge's headless dedicated server on the sequel's own
// protocol, the engine's dedicated process (engine/server/main.hpp: server.cfg, the flags, the run
// loop, the status lines) over the sequel's server game (net/sequel_game.hpp), so an existing
// client of the sequel connects and plays. `--help` lists the flags.
//
// ge:summary The sequel's headless dedicated server: the engine's process over the sequel's game
// ge:keywords dedicated-server shooters_edge_sequel_server sequel protocol-34 headless main
// ge:skill shooter net
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "server/main.hpp"
#include "shooters_edge/net/sequel_game.hpp"

int main(int argc, char** argv) {
  return ge::server::server_main(argc, argv, shooters_edge::net::sequel_game());
}
