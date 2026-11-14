// The sequel's screens (server/sequel_screens.hpp): the scoreboard and the help computer as layout
// programs, the commands that show and hide them, their refresh, the wire's layout message and
// the window drawing them over the bar.
//
// ge:summary Tests: the sequel's scoreboard and help computer, their commands, wire and drawing
// ge:keywords sequel scoreboard help-computer layout screen test
// ge:skill testing shooter
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/sequel_bar.hpp"
#include "shooters_edge/server/sequel_bar.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_screens.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"

namespace {

namespace br = ge::brush;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
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

bool has(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST(score_toggles_the_scoreboard_with_the_viewers_dog_tag) {
  auto g = game("1");
  run(*g, 20);
  g->p(1).frags = 4;
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);
  const std::string& lay = g->seen(0).layout;
  CHECK(g->p(0).screen == sv::kScreenScores && (g->seen(0).numbers.stats[13] & 1) == 1);
  // Ben leads in the first block, Ana under him with her tag behind her name.
  CHECK_MSG(has(lay, "client 0 32 1 4 ") && has(lay, "xv 32 yv 64 picn tag1 client 0 64 0 0 "),
            lay);
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);
  CHECK(g->p(0).screen == sv::kScreenNone && g->seen(0).numbers.stats[13] == 0);
  CHECK(g->seen(0).layout == lay);  // the client keeps the last one, undrawn
}

TEST(twelve_blocks_in_two_columns_and_the_rest_left_out) {
  auto g = game("1", 14);
  run(*g, 20);
  for (int i = 0; i < 14; i++) g->p(i).frags = static_cast<float>(14 - i);
  const std::string lay = sv::sequel_scoreboard(g->s(), -1, -1);
  CHECK_MSG(has(lay, "client 0 32 0 14 ") && has(lay, "client 0 192 5 9 "), lay);
  CHECK_MSG(has(lay, "client 160 32 6 8 ") && has(lay, "client 160 192 11 3 "), lay);
  CHECK(!has(lay, " 12 2 ") && !has(lay, "client 160 224"));
  CHECK(lay.size() <= sv::kLayoutMost);
}

TEST(an_open_scoreboard_is_written_again_every_32_frames) {
  auto g = game("1");
  run(*g, 20);
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);
  g->p(1).frags = 7;
  const int64_t at = g->s().frame_number;
  while (g->s().frame_number < at + 33) run(*g, 1);
  CHECK_MSG(has(g->seen(0).layout, "client 0 32 1 7 "), g->seen(0).layout);
}

TEST(dying_in_deathmatch_shows_the_killer_and_respawning_closes_it) {
  auto g = game("1");
  run(*g, 20);
  sv::fight_kill(g->s(), 0, 1, -1);
  run(*g, 1);
  CHECK(g->p(0).screen == sv::kScreenScores);
  CHECK_MSG(has(g->seen(0).layout, "xv 32 yv 64 picn tag2 client 0 64 1 "), g->seen(0).layout);
  sv::fight_spawn(g->s(), 0);
  run(*g, 1);
  CHECK(g->p(0).screen == sv::kScreenNone);
}

TEST(help_in_single_player_shows_the_help_computer_and_score_closes_it) {
  auto g = game("0", 1);
  run(*g, 20);
  g->s().help[0] = "Find the \"data\" cd.";
  CHECK(g->r.client_command(0, "help") == sv::CommandResult::mod);
  const std::string& lay = g->seen(0).layout;
  CHECK(g->p(0).screen == sv::kScreenHelp && g->seen(0).numbers.stats[13] == 1);
  CHECK_MSG(has(lay, "xv 32 yv 8 picn help ") && has(lay, "yv 24 cstring2 \"hall\"") &&
                has(lay, "yv 54 cstring2 \"Find the data cd.\""),
            lay);
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);  // no scores alone
  CHECK(g->p(0).screen == sv::kScreenNone);
  CHECK(g->r.client_command(0, "help") == sv::CommandResult::mod);
  CHECK(g->r.client_command(0, "putaway") == sv::CommandResult::mod);
  CHECK(g->p(0).screen == sv::kScreenNone);
}

TEST(the_wire_sends_the_layout_once_when_it_changes) {
  auto g = game("1");
  run(*g, 20);
  sv::SequelWire w;
  sv::SequelSignon signon;
  sv::sequel_signon_parts(w, *g->r, 0, {}, signon);
  std::vector<uint8_t> block, reliable;
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);
  sv::sequel_frame(w, *g->r, int32_t(g->s().frame_number), nullptr, block, &reliable);
  const std::string lay = g->seen(0).layout;
  std::vector<uint8_t> want{4};
  want.insert(want.end(), lay.begin(), lay.end());
  want.push_back(0);
  CHECK(std::search(reliable.begin(), reliable.end(), want.begin(), want.end()) != reliable.end());
  reliable.clear();
  sv::sequel_frame(w, *g->r, int32_t(g->s().frame_number), nullptr, block, &reliable);
  CHECK(std::search(reliable.begin(), reliable.end(), want.begin(), want.end()) == reliable.end());
}

TEST(the_window_draws_the_scoreboard_over_the_bar_only_while_it_is_up) {
  auto g = game("1");
  run(*g, 20);
  cl::SequelBar bar;
  cl::run_sequel_bar(bar, g->r->world, 0, g->s().frame_number, 640, 480);
  const size_t bare = bar.ops.size();
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);
  cl::run_sequel_bar(bar, g->r->world, 0, g->s().frame_number, 640, 480);
  const bool named =
      std::any_of(bar.ops.begin(), bar.ops.end(), [](const auto& op) { return op.text == "Ana"; });
  CHECK_MSG(bar.ops.size() > bare && named, std::to_string(bar.ops.size()));
  CHECK(g->r.client_command(0, "score") == sv::CommandResult::mod);
  cl::run_sequel_bar(bar, g->r->world, 0, g->s().frame_number, 640, 480);
  CHECK(bar.ops.size() == bare);
}
