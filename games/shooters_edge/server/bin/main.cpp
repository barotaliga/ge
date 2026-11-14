// `sesv`: Shooter's Edge's headless dedicated server, the engine's dedicated process
// (engine/server/main.hpp: server.cfg, the flags, the run loop, the status lines, the digest) over
// one of the game's playable ServerGames. By default the classic one (net/classic_game.hpp): the
// mod's deathmatch served over the classic protocol, so a classic client connects and plays.
// `-game 2` (or `--game=2`, anywhere on the line) serves the sequel's rules on its own protocol
// instead (net/sequel_game.hpp), so an existing client of the sequel connects and plays; `-game 1`
// is the default. `--help` lists the flags.
//
// voice: agent (a command-line tool's main: its errors are for whoever runs it;
// docs/architecture.md § Player text).
//
// ge:summary The dedicated server `sesv`: the engine's process over the classic or sequel game
// ge:keywords dedicated-server sesv headless main classic-protocol server-cfg sequel protocol-34
// ge:keywords game-number
// ge:skill shooter net
// ge:test tests/shooters_edge_classic_game_test.cpp
#include <cstdio>
#include <cstring>
#include <vector>

#include "server/main.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/sequel_game.hpp"

// `-game <n>` or `--game=<n>`: which classic game the server serves, by its number.
int game_number(int argc, char** argv, std::vector<char*>& rest, bool& help) {
  int number = 1;
  for (int i = 0; i < argc; i++) {
    const char* value = nullptr;
    if (i > 0 && std::strcmp(argv[i], "-game") == 0 && i + 1 < argc) {
      value = argv[++i];
    } else if (i > 0 && std::strncmp(argv[i], "--game=", 7) == 0) {
      value = argv[i] + 7;
    } else {
      if (std::strcmp(argv[i], "--help") == 0) help = true;
      rest.push_back(argv[i]);
      continue;
    }
    number = std::strcmp(value, "1") == 0 ? 1 : std::strcmp(value, "2") == 0 ? 2 : 0;
    if (number == 0) {
      std::fprintf(stderr, "sesv: -game %s is no game; next: -game 1 or -game 2\n", value);
      return 0;
    }
  }
  return number;
}

int main(int argc, char** argv) {
  bool help = false;
  std::vector<char*> args;
  const int number = game_number(argc, argv, args, help);
  if (number == 0) return 1;
  if (help && number == 1)
    std::puts("sesv [-game 1|2] [flags]: -game 2 serves the sequel's rules on its own protocol\n");
  const ge::server::ServerGame& game =
      number == 2 ? shooters_edge::net::sequel_game() : shooters_edge::net::classic_game();
  return ge::server::server_main(static_cast<int>(args.size()), args.data(), game);
}
