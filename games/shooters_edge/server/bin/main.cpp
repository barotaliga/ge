// `sesv`: Shooter's Edge's headless dedicated server, the engine's dedicated process
// (engine/server/main.hpp: server.cfg, the flags, the run loop, the status lines, the digest) over
// one of the game's playable ServerGames, the one `fs_basegame` names on the command line
// (`+set fs_basegame q2`, client/installs.hpp). By default (`q1`) QuakeWorld's (net/qw_game.hpp):
// the mod's deathmatch served over the QuakeWorld protocol, so a QuakeWorld client connects and
// plays; `q2` serves Quake II's rules on its own protocol (net/q2_game.hpp), so an existing client
// of Quake II connects and plays. Its install is `fs_basedir_<id>` when `--root` is not given.
// `-game <folder>` runs a mod's folder over the base game, as each classic game names one (the
// engine's `--mod=<folder>`). `--help` lists the flags.
//
// voice: agent (a command-line tool's main: its errors are for whoever runs it;
// docs/architecture.md § Player text).
//
// ge:summary The dedicated server `sesv`: the engine's process over Quake's or Quake II's game
// ge:keywords dedicated-server sesv headless main qw-protocol server-cfg q2 protocol-34
// ge:keywords fs_basegame fs_basedir game-folder
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

// The value `+set <name> <value>` (or `+<name> <value>`) gives on the command line, or "".
std::string plus_value(int argc, char** argv, std::string_view name) {
  std::string out;
  for (int i = 1; i + 1 < argc; i++) {
    const bool set = (std::strcmp(argv[i], "+set") == 0 || std::strcmp(argv[i], "+seta") == 0);
    if (set && i + 2 < argc && name == argv[i + 1]) {
      out = argv[i + 2];
    } else if (argv[i][0] == '+' && name == argv[i] + 1) {
      out = argv[i + 1];
    }
  }
  return out;
}

// Which classic game the server serves, as its number from 1 (`fs_basegame`: "" or q1 is 1, q2
// is 2); 0 after saying why. `-game <folder>` is passed on as the engine's `--mod=<folder>`, and
// `fs_basedir_<id>` as `--root=<folder>` when no `--root` is given; every other word as it stands.
int game_number(int argc, char** argv, std::vector<char*>& rest, std::vector<std::string>& owned,
                bool& help) {
  const std::string id = plus_value(argc, argv, "fs_basegame");
  const int number = id.empty() || id == "q1" ? 1 : id == "q2" ? 2 : 0;
  if (number == 0) {
    std::fprintf(stderr, "sesv: fs_basegame %s is no game; next: +set fs_basegame q1 or q2\n",
                 id.c_str());
    return 0;
  }
  bool rooted = false;
  for (int i = 0; i < argc; i++) {
    if (i > 0 && std::strcmp(argv[i], "-game") == 0 && i + 1 < argc) {
      owned.push_back(std::string("--mod=") + argv[++i]);
      continue;
    }
    if (std::strcmp(argv[i], "--help") == 0) help = true;
    if (std::strncmp(argv[i], "--root=", 7) == 0) rooted = true;
    rest.push_back(argv[i]);
  }
  if (const std::string dir =
          plus_value(argc, argv, number == 2 ? "fs_basedir_q2" : "fs_basedir_q1");
      !rooted && !dir.empty())
    owned.push_back("--root=" + dir);
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
        "sesv [-game <folder>] [flags] [+set fs_basegame q1|q2] [+set fs_basedir_<id> <folder>]: "
        "fs_basegame q2 serves Quake II's rules on its own protocol, fs_basedir_<id> is its "
        "install (when no --root); -game runs a mod's folder over the base game\n");
  const ge::server::ServerGame& game =
      number == 2 ? shooters_edge::net::q2_game() : shooters_edge::net::qw_game();
  // The reference manual (`--manual=PATH`): the server's games, this run's pass and its own flags.
  ge::console::ManualInfo manual;
  manual.program = {"sesv", "Dedicated server", {}};
  manual.games = {{"qw", "QuakeWorld", {}}, {"q2", "Quake II", {}}};
  manual.pass = number == 2 ? "q2" : "qw";
  manual.flags = {
      {"-game", "<folder>", "Run a mod's folder over the base game (the same as --mod=<folder>)."}};
  // The rules' own settings (deathmatch, coop...), which the console hands to the game's rules.
  manual.more = [](std::string_view games, std::vector<ge::console::ManualEntry>& out) {
    shooters_edge::server::rules_manual(games, out);
  };
  return ge::server::server_main(static_cast<int>(args.size()), args.data(), game, manual);
}
