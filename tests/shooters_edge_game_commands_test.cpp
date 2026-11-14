// The game's own commands from the window's console (net/game_commands.hpp): `cmd` and the
// sequel's screen words reach the sequel's rules, do nothing in another game, and the classic
// scores board stays off in the sequel, whose rules draw its own screens.
//
// ge:summary Tests: cmd and the sequel's words from the window's console, TAB left to the sequel
// ge:keywords cmd game-commands sequel score inven console-words test
// ge:skill testing shooter console
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/game_commands.hpp"
#include "shooters_edge/server/sequel_screens.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p(int i) const { return s().players[i]; }
  const sv::WorldClient& seen(int i) const { return r->world.clients[i]; }
};

std::unique_ptr<Game> game(const char* deathmatch, int players = 2) {
  auto g = std::make_unique<Game>();
  g->r = sv::sequel_runner(sv::make_sequel_server({}));
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

// The window's console over the sequel's game (or none): the listen points at its rules.
struct Window {
  std::unique_ptr<Game> g;
  cl::Listen l;
  sn::Remote r;
  ge::console::Console con{false};
  explicit Window(bool sequel) {
    if (sequel) {
      g = game("1");
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

TEST(the_sequels_words_open_its_screens_from_the_console) {
  Window w(true);
  CHECK(cl::listen_is_sequel(w.l));
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

TEST(outside_the_sequel_its_words_do_nothing_and_cmd_says_so) {
  Window w(false);
  CHECK(!cl::listen_is_sequel(w.l));
  for (const char* word : sn::kSequelWords) w.con.run_now(word);
  CHECK(w.con.output().empty());  // quiet: the sequel's config may bind a key to one
  w.con.run_now("cmd help");
  CHECK(printed(w.con, "You need to be in a game for that."));
}

TEST(the_classic_board_stays_off_in_the_sequel) {
  Window w(true);
  CHECK(cl::scores_screen(w.l, true) == cl::ScoresScreen::none);
}
