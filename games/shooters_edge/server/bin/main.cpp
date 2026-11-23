// `sesv`: Shooter's Edge's headless dedicated server, the engine's dedicated process
// (engine/server/main.hpp: server.cfg, the flags, the run loop, the status lines, the digest) over
// one of the game's playable ServerGames. By default QuakeWorld's (net/qw_game.hpp): the
// mod's deathmatch served over the QuakeWorld protocol, so a QuakeWorld client connects and plays.
// `-classic q2` (or `--classic=q2`, anywhere on the line) serves Quake II's rules on its own
// protocol instead (net/q2_game.hpp), so an existing client of Quake II connects and plays;
// `-classic q1` is the default. `-game <folder>` runs a mod's folder over the base game, as each
// classic game names one (the engine's `--mod=<folder>`). `--help` lists the flags.
//
// voice: agent (a command-line tool's main: its errors are for whoever runs it;
// docs/architecture.md § Player text).
//
// ge:summary The dedicated server `sesv`: the engine's process over Quake's or Quake II's game
// ge:keywords dedicated-server sesv headless main qw-protocol server-cfg q2 protocol-34
// ge:keywords classic-id game-folder
// ge:skill shooter net
// ge:test tests/shooters_edge_qw_game_test.cpp
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "server/main.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/server/rules_manual.hpp"

// `-classic <id>` or `--classic=<id>`: which classic game the server serves (q1, q2), as its
// number from 1; 0 after saying why. `-game <folder>` is passed on as the engine's
// `--mod=<folder>`; every other word is passed on as it stands.
int game_number(int argc, char** argv, std::vector<char*>& rest, std::vector<std::string>& owned,
                bool& help) {
  int number = 1;
  for (int i = 0; i < argc; i++) {
    const char* value = nullptr;
    if (i > 0 && std::strcmp(argv[i], "-game") == 0 && i + 1 < argc) {
      owned.push_back(std::string("--mod=") + argv[++i]);
      continue;
    }
    if (i > 0 && std::strcmp(argv[i], "-classic") == 0 && i + 1 < argc) {
      value = argv[++i];
    } else if (i > 0 && std::strncmp(argv[i], "--classic=", 10) == 0) {
      value = argv[i] + 10;
    } else {
      if (std::strcmp(argv[i], "--help") == 0) help = true;
      rest.push_back(argv[i]);
      continue;
    }
    number = std::strcmp(value, "q1") == 0 ? 1 : std::strcmp(value, "q2") == 0 ? 2 : 0;
    if (number == 0) {
      std::fprintf(stderr, "sesv: -classic %s is no game; next: -classic q1 or -classic q2\n",
                   value);
      return 0;
    }
  }
  return number;
}

int main(int argc, char** argv) {
  bool help = false;
  std::vector<char*> args;
  std::vector<std::string> owned;
  owned.reserve(static_cast<size_t>(argc));  // `args` points into it: never reallocated
  const int number = game_number(argc, argv, args, owned, help);
  if (number == 0) return 1;
  for (std::string& w : owned) args.push_back(w.data());
  if (help && number == 1)
    std::puts(
        "sesv [-classic q1|q2] [-game <folder>] [flags]: -classic q2 serves Quake II's rules on "
        "its own protocol; -game runs a mod's folder over the base game\n");
  const ge::server::ServerGame& game =
      number == 2 ? shooters_edge::net::q2_game() : shooters_edge::net::qw_game();
  // The reference manual (`--manual=PATH`): the server's games, this run's pass and its own flags.
  ge::console::ManualInfo manual;
  manual.program = {"sesv", "Dedicated server", {}};
  manual.games = {{"qw", "QuakeWorld", {}}, {"q2", "Quake II", {}}};
  manual.pass = number == 2 ? "q2" : "qw";
  manual.flags = {
      {"-classic", "<id>",
       "Serve that classic game: q1 (QuakeWorld's deathmatch on its protocol, the default) or q2 "
       "(Quake II's rules on its own protocol). Also --classic=<id>."},
      {"-game", "<folder>", "Run a mod's folder over the base game (the same as --mod=<folder>)."}};
  // The rules' own settings (deathmatch, coop...), which the console hands to the game's rules.
  manual.more = [](std::string_view games, std::vector<ge::console::ManualEntry>& out) {
    shooters_edge::server::rules_manual(games, out);
  };
  return ge::server::server_main(static_cast<int>(args.size()), args.data(), game, manual);
}
