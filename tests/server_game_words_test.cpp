// A game's declared console words (server/main.hpp's add_game_words): registered in the game's
// own layer, so Tab and the manual know them; run, the whole line goes to the game; a word the
// console has already stays the console's; the game's other spelling is a synonym.
//
// ge:summary Tests a game's declared server words: registered, run, skipped, spelled, described
// ge:keywords game-words add_game_words server-words manual tab-completion test
// ge:skill net console testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "console/manual.hpp"
#include "server/main.hpp"

namespace gs = ge::server;
namespace cn = ge::console;

namespace {

constexpr gs::GameWord kWords[] = {
    {"spin", "spin <turns>: turn the ring"},
    {"stop_ring", "stop_ring: stop the ring", "halt"},
    {"echo", "echo <text>: the game's own echo, never registered"},
};

gs::ServerGame game() {
  gs::ServerGame g;
  g.words = kWords;
  g.word_count = 3;
  return g;
}

}  // namespace

TEST(a_games_words_run_the_game_with_the_whole_line) {
  cn::Console con(true);
  con.set_manual({.program = {"sv", "Server", {}},
                  .games = {{"ring", "Ring", {}}},
                  .pass = "ring",
                  .flags = {}});
  const gs::ServerGame g = game();
  std::vector<std::string> lines;
  gs::GameWordRun answer = gs::GameWordRun::taken;
  gs::add_game_words(con, g, [&](const std::string& line) {
    lines.push_back(line);
    return answer;
  });
  std::vector<std::string> printed;
  con.set_sink([&](std::string_view s) { printed.emplace_back(s); });
  con.execute("spin 3");
  con.execute("halt");
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "spin 3");
  CHECK(lines[1] == "halt");  // the game's spelling reaches the game as typed
  answer = gs::GameWordRun::no_game;
  con.execute("spin 1");
  answer = gs::GameWordRun::refused;
  con.execute("spin");
  CHECK(std::find(printed.begin(), printed.end(), "spin: no map is running") != printed.end());
  CHECK(std::find(printed.begin(), printed.end(), "spin: the game did not take that line") !=
        printed.end());
  // The console's own echo stays its own: the game never hears it.
  const size_t heard = lines.size();
  con.execute("echo hello");
  CHECK(lines.size() == heard);
}

TEST(a_games_words_are_its_own_layers_in_the_manual) {
  cn::Console con(true);
  con.set_manual({.program = {"sv", "Server", {}},
                  .games = {{"ring", "Ring", {}}},
                  .pass = "ring",
                  .flags = {}});
  gs::add_game_words(con, game(), [](const std::string&) { return gs::GameWordRun::taken; });
  bool spin = false, halt = false, echo_ours = false;
  for (const cn::ManualEntry& e : cn::describe(con, "sv", "ring")) {
    if (e.kind == cn::ManualKind::command && e.name == "spin")
      spin = e.layer == "game:ring" && e.help == "spin <turns>: turn the ring";
    if (e.kind == cn::ManualKind::command_synonym && e.name == "halt")
      halt = e.target == "stop_ring" && e.layer == "game:ring";
    if (e.kind == cn::ManualKind::command && e.name == "echo")
      echo_ours = e.help.find("game's own") == std::string::npos;
  }
  CHECK(spin);
  CHECK(halt);
  CHECK(echo_ours);
}
