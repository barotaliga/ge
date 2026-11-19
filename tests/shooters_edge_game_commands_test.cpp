// The game's own commands from the window's console (net/game_commands.hpp): `cmd` and the
// Quake II's screen words reach Quake II's rules, do nothing in another game, and the classic
// scores board stays off in Quake II, whose rules draw its own screens; Quake II's game saves
// from the window and its autosave is taken once.
//
// ge:summary Tests: cmd and Quake II's words from the window's console, TAB left to Quake II
// ge:keywords cmd game-commands q2 score inven console-words test
// ge:keywords unknown-words listen_rules_word listen_save listen_take_autosave
// ge:skill testing shooter console
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/net/game_commands.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_screens.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace sn = shooters_edge::net;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "300 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Game {
  sv::RulesRunner r;
  int players = 0;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
  const sv::WorldClient& seen(int i) const { return r->world.clients[i]; }
};

std::unique_ptr<Game> game(const char* deathmatch, int players = 2) {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
  g->r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(g->r.start(map, error), error);
  static const char* kNames[] = {"Ana", "Ben", "Cy", "Di",  "Ed", "Flo", "Gus",
                                 "Hal", "Ivy", "Jo", "Kai", "Lu", "Max", "Ned"};
  for (int i = 0; i < players; i++) g->r.connect(i, kNames[i], false, false);
  g->players = players;
  return g;
}

// Host ticks of 13 ms, every player standing still.
void run(Game& g, int ticks) {
  for (int t = 0; t < ticks; t++) {
    std::vector<sh::Packet> packets(static_cast<size_t>(g.players));
    for (int c = 0; c < g.players; c++) {
      sh::Packet& p = packets[static_cast<size_t>(c)];
      p.client = c;
      p.sequence = ++g.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd;
    }
    g.r.frame(packets.data(), g.players);
  }
}

// The window's console over Quake II's game (or none): the listen points at its rules.
struct Window {
  std::unique_ptr<Game> g;
  cl::Listen l;
  sn::Remote r;
  ge::console::Console con{false};
  explicit Window(bool q2, const char* deathmatch = "1") {
    if (q2) {
      g = game(deathmatch, deathmatch[0] == '0' ? 1 : 2);
      run(*g, 20);
      l.rules = &g->r;
      l.server = g->r.common;
    }
    sn::add_server_commands(con, l, r);
  }
};

bool printed(const ge::console::Console& c, const std::string& part) {
  for (const std::string& line : c.output())
    if (line.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(q2_words_open_its_screens_from_the_console) {
  Window w(true);
  CHECK(cl::listen_is_q2(w.l));
  w.con.run_now("score");
  CHECK(w.g->p(0).screen == sv::kScreenScores);
  w.con.run_now("score");
  CHECK(w.g->p(0).screen == sv::kScreenNone);
  w.con.run_now("inven");
  CHECK(w.g->p(0).inven_open);
  w.con.run_now("putaway");
  CHECK(!w.g->p(0).inven_open);
}

TEST(cmd_sends_the_line_to_the_rules_as_it_is) {
  Window w(true);
  w.con.run_now("cmd score");
  CHECK(w.g->p(0).screen == sv::kScreenScores);
  w.con.clear_output();
  w.con.run_now("cmd");
  CHECK(printed(w.con, "type the command to send after it"));
}

TEST(outside_q2_its_words_do_nothing_and_cmd_says_so) {
  Window w(false);
  CHECK(!cl::listen_is_q2(w.l));
  for (const char* word : sn::kQ2Words) w.con.run_now(word);
  CHECK(w.con.output().empty());  // quiet: Quake II's config may bind a key to one
  w.con.run_now("cmd help");
  CHECK(printed(w.con, "You need to be in a game for that."));
}

TEST(the_classic_board_stays_off_in_the_q2) {
  Window w(true);
  CHECK(cl::scores_screen(w.l, true) == cl::ScoresScreen::none);
}

TEST(a_line_the_window_does_not_know_goes_to_q2_rules) {
  // As the window wires it (client/main.cpp): `players`, which no console command takes, reaches
  // the rules; a word the rules do not know either is said to everyone, as Quake II's rules
  // take any line (server/q2_commands.hpp).
  Window w(true);
  w.con.set_unknown([&w](const ge::console::Args& a) { return cl::listen_rules_word(w.l, a.raw); });
  w.con.run_now("players");
  CHECK(!printed(w.con, "unknown command"));
  w.con.run_now("a_word_nobody_knows");
  CHECK(!printed(w.con, "unknown command"));
  bool said = false;
  for (const sv::NetEvent& e : w.g->s().out.events)
    said = said || (e.kind == sv::EventKind::print && e.text == "Ana: a_word_nobody_knows\n");
  CHECK(said);
  // Outside Quake II nothing is passed on.
  Window other(false);
  CHECK(!cl::listen_rules_word(other.l, "players"));
}

TEST(q2_game_saves_from_the_window_and_its_autosave_is_taken_once) {
  Window alone(true, "0");
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(cl::listen_save(alone.l, file, error), error);
  CHECK(sv::q2_save_map(file, "s0", error) == "hall");
  CHECK(!cl::listen_take_autosave(alone.l).empty());  // made as the map was entered
  CHECK(cl::listen_take_autosave(alone.l).empty());
  Window match(true);
  CHECK(!cl::listen_save(match.l, file, error));
  CHECK_MSG(error.find("deathmatch") != std::string::npos, error);
  CHECK(cl::listen_take_autosave(match.l).empty());  // none in deathmatch
}
