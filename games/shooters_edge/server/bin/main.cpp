// `sesv`: Shooter's Edge's headless dedicated server, the engine's dedicated process
// (engine/server/main.hpp: server.cfg, the flags, the run loop, the status lines, the digest) over
// one of the game's playable ServerGames. By default the classic one (net/classic_game.hpp): the
// mod's deathmatch served over the classic protocol, so a classic client connects and plays.
// `--sequel` (anywhere on the line) serves the sequel's rules on its own protocol instead
// (net/sequel_game.hpp), so an existing client of the sequel connects and plays. `--help` lists
// the flags.
//
// ge:summary The dedicated server `sesv`: the engine's process over the classic or sequel game
// ge:keywords dedicated-server sesv headless main classic-protocol server-cfg sequel protocol-34
// ge:keywords sesv-sequel
// ge:skill shooter net
// ge:test tests/shooters_edge_classic_game_test.cpp
#include <cstdio>
#include <cstring>
#include <vector>

#include "server/main.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/sequel_game.hpp"

int main(int argc, char** argv) {
  bool sequel = false, help = false;
  std::vector<char*> args;
  for (int i = 0; i < argc; i++) {
    if (i > 0 && std::strcmp(argv[i], "--sequel") == 0) {
      sequel = true;
      continue;
    }
    if (std::strcmp(argv[i], "--help") == 0) help = true;
    args.push_back(argv[i]);
  }
  if (help && !sequel)
    std::puts("sesv [--sequel] [flags]: --sequel serves the sequel's rules on its own protocol\n");
  const ge::server::ServerGame& game =
      sequel ? shooters_edge::net::sequel_game() : shooters_edge::net::classic_game();
  return ge::server::server_main(static_cast<int>(args.size()), args.data(), game);
}
