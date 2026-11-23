// The window's part of the reference manual, defined (manual.hpp).
//
// ge:summary The window's part of the reference manual defined: every game's pass, the file
// ge:keywords manual describe_games write_manual manual_info manual_pass
// ge:skill console shooter
// ge:test tests/shooters_edge_manual_test.cpp
#include "shooters_edge/client/manual.hpp"

#include "core/output.hpp"
#include "shooters_edge/client/args.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

con::ManualInfo manual_info() {
  con::ManualInfo m;
  m.program = {.id = "se", .name = "Shooter's Edge", .takes_in = {}};
  for (const profiles::GameProfile& g : profiles::kGameProfiles) {
    m.games.push_back({.id = std::string(g.id),
                       .name = std::string(profiles::content_of(g).what),
                       .takes_in = {}});
    if (g.id == "q1") m.games.push_back({.id = "qw", .name = "QuakeWorld", .takes_in = {}});
  }
  m.flags = flag_docs();
  return m;
}

std::string manual_pass(const profiles::GameProfile& game) {
  return game.id == "q1" ? "q1,qw" : std::string(game.id);
}

con::ManualSource describe_games(con::Console& c, GameCvarValues& v, const GameBindings& b,
                                 GameStart& start, const profiles::GameProfile& loaded) {
  c.set_manual(manual_info());
  con::ManualSource out;
  out.programs = {c.manual().program};
  out.games = c.manual().games;
  const profiles::GameProfile* now = &loaded;
  const auto load = [&](const profiles::GameProfile& g) {
    if (&g == now) return;
    start_switch_game(c, start, *now, g);
    switch_game_cvars(c, v, g, b);
    now = &g;
  };
  for (const profiles::GameProfile& g : profiles::kGameProfiles) {
    load(g);
    c.manual().pass = manual_pass(g);
    const std::vector<con::ManualEntry> pass = con::describe(c, "se", c.manual().pass);
    out.entries.insert(out.entries.end(), pass.begin(), pass.end());
  }
  load(loaded);
  c.manual().pass = manual_pass(loaded);
  return out;
}

int write_manual(con::Console& c, GameCvarValues& v, const GameBindings& b, GameStart& start,
                 const profiles::GameProfile& loaded, const std::string& path) {
  std::string error;
  if (!con::write_manual(path, describe_games(c, v, b, start, loaded), error)) {
    ge::core::output::error("se: " + error + " next: name a file in a folder you can write");
    return 1;
  }
  ge::core::output::info("se: the reference manual's source written to " + path);
  return 0;
}

}  // namespace shooters_edge::client
