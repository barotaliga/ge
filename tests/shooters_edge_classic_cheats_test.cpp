// The classic server's cheat check (games/shooters_edge/net/classic_cheats.hpp): a client whose
// commands claim half again the time that passed is cut by the speed check every second and is out
// within seconds; an honest client, a bot and a client whose packets bunch after a stall are
// never flagged; a slot taken by a new client starts clean; and an out player's verdict reaches
// the match report's `flagged`.
//
// ge:summary Tests the classic server's cheat check: a speed hack out, honest players clean
// ge:keywords anti-cheat speed-hack speed-check cheat-watch flagged match-report
// ge:skill net shooter
#include <array>
#include <string>

#include "check.hpp"
#include "net/report.hpp"
#include "shooter/client.hpp"
#include "shooter_loop_bench.hpp"
#include "shooters_edge/net/classic_cheats.hpp"
#include "shooters_edge/net/classic_report.hpp"
#include "shooters_edge/server/rules_world.hpp"

namespace gn = ge::net;
namespace sh = ge::shooter;
namespace se = shooters_edge::net;
namespace sv = shooters_edge::server;
using ge::test::loop::Bench;

namespace {

sh::MoveCmd cmd(int msec) {
  sh::MoveCmd c;
  c.msec = static_cast<uint8_t>(msec);
  return c;
}

// The server's 13 ms frame: the time moves on and each player's command for it runs.
struct Field {
  Bench b;
  se::CheatWatch watch;
  Field() {
    b.start();
    sh::connect_client(b.sv, 0, false, false);  // honest
    sh::connect_client(b.sv, 1, false, false);  // a clock half again too fast
    sh::connect_client(b.sv, 2, false, true);   // a bot: never checked
    sh::connect_client(b.sv, 3, false, false);  // honest, but its packets stall and bunch
    se::cheat_watch_frame(watch, b.sv);
  }
  // Frame `f` of the run: slot 3 sends nothing over frames [stall_from, stall_from + 150) and
  // all of it at once at their end (2 s of commands in one burst).
  void frame(int f, int stall_from) {
    b.sv.time += 0.013;
    sh::run_player_command(b.sv, 0, cmd(13));
    sh::run_player_command(b.sv, 1, cmd(f % 2 == 0 ? 20 : 19));
    sh::run_player_command(b.sv, 2, cmd(26));
    if (f < stall_from || f >= stall_from + 150) sh::run_player_command(b.sv, 3, cmd(13));
    if (f == stall_from + 150)
      for (int k = 0; k < 150; k++) sh::run_player_command(b.sv, 3, cmd(13));
    se::cheat_watch_frame(watch, b.sv);
  }
};

}  // namespace

TEST(classic_cheats_a_fast_clock_is_out_and_honest_players_are_not) {
  Field w;
  int out_at = -1;
  for (int f = 0; f < 77 * 20; f++) {  // 20 s
    w.frame(f, 77 * 8);
    if (out_at < 0 && se::cheat_watch_out(w.watch, 1)) out_at = f;
  }
  CHECK_MSG(out_at > 0 && out_at < 77 * 8,
            "the fast clock was out at frame " + std::to_string(out_at) + " (want under 8 s)");
  const std::string v = se::cheat_watch_verdict(w.watch, 1);
  CHECK_MSG(v.rfind("flood x", 0) == 0 && v.find("; out") != std::string::npos, v);
  CHECK(w.b.sv.clients[1].speed_cut_ms > 5000);
  for (int slot : {0, 2, 3})
    CHECK_MSG(se::cheat_watch_verdict(w.watch, slot).empty(),
              "slot " + std::to_string(slot) + ": " + ge::net::guard_summary(w.watch.guard, slot));
  CHECK(w.b.sv.clients[2].speed_cut_ms == 0);
  CHECK_MSG(w.b.sv.clients[3].speed_cut_ms > 0, "the stall's burst was cut, once");
}

TEST(classic_cheats_a_new_client_in_the_slot_starts_clean) {
  Field w;
  for (int f = 0; f < 77 * 10; f++) w.frame(f, 1 << 30);
  CHECK(se::cheat_watch_out(w.watch, 1));
  sh::disconnect_client(w.b.sv, 1);
  se::cheat_watch_frame(w.watch, w.b.sv);
  sh::connect_client(w.b.sv, 1, false, false);
  for (int f = 0; f < 77 * 3; f++) {
    w.b.sv.time += 0.013;
    sh::run_player_command(w.b.sv, 1, cmd(13));
    se::cheat_watch_frame(w.watch, w.b.sv);
  }
  CHECK_MSG(se::cheat_watch_verdict(w.watch, 1).empty(), ge::net::guard_summary(w.watch.guard, 1));
}

TEST(classic_cheats_an_out_player_is_flagged_in_the_match_report) {
  sv::RulesWorld world;
  world.map_name = "arena";
  for (int i = 0; i < 2; i++) {
    sv::WorldClient& c = world.clients[i];
    c.active = true;
    c.userid = i + 1;
    c.name = i == 0 ? "ann" : "bo";
    c.userinfo = {{"name", c.name}};
    c.numbers.health = 100;
  }
  std::array<uint64_t, sh::kMaxClients> accounts{};
  accounts[1] = 42;
  se::MatchTally t;
  world.time = 1;
  se::tally_frame(t, world, true, accounts);
  world.time = 30;
  se::tally_frame(t, world, true, accounts);
  se::tally_flag(t, 1, "flood x5; worst score 46.0; out");
  se::TallyFacts facts;
  facts.mode = "1on1";
  gn::MatchReport r;
  CHECK(se::tally_end(t, world, facts, r));
  CHECK(r.players.size() == 2);
  for (const gn::ReportPlayer& p : r.players)
    CHECK_MSG(p.flagged == (p.name == "bo" ? "flood x5; worst score 46.0; out" : ""),
              p.name + ": '" + p.flagged + "'");
}
