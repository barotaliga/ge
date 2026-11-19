// LAB `se-cheats`: the classic server's cheat check (games/shooters_edge/net/qw_cheats.hpp)
// over a run of players on one server's move: an honest player, clocks running fast by a half and
// by a tenth, a clock a few percent fast, a player whose packets stall for two seconds and arrive
// in one burst, and a bot. Each sends its commands on the server's 13 ms frame by its own clock;
// the speed check cuts what runs ahead and the watch scores the cuts once a second. The lab
// answers whether each fast clock is out and how soon, and whether anyone honest is flagged; the
// scores are charted over the run with the out score ruled across.
//
// ge:summary Lab: the classic server's speed check scored: fast clocks out, honest players clean
// ge:keywords lab anti-cheat speed-hack speed-check cheat-watch flagged classic-server
// ge:skill net shooter labs
// ge:test tests/shooters_edge_qw_cheats_test.cpp
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/guard.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/client.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/net/qw_cheats.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace sh = ge::shooter;
namespace se = shooters_edge::net;

struct Player {
  const char* who;
  double rate;     // the client's clock against the server's (1: honest)
  bool bot;        // never checked
  bool stalls;     // sends nothing for two seconds at the run's middle, then all of it at once
  bool cheat;      // must be out
  double owed_ms;  // the client's own time not yet sent as a command
};

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

sh::Brush floor_box() {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, 1024, 0},  {{-1, 0, 0}, 1024, 3}, {{0, 1, 0}, 1024, 1},
              {{0, -1, 0}, 1024, 3}, {{0, 0, 1}, 0, 2},     {{0, 0, -1}, 64, 3}};
  return b;
}

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  if (seconds < 6) {
    lab.fail("--seconds is under 6", "Pass --seconds=6 or more; skill: net.");
    return;
  }
  std::vector<Player> players = {
      {"honest", 1.0, false, false, false, 0},      {"fast x1.5", 1.5, false, false, true, 0},
      {"fast x1.1", 1.1, false, false, true, 0},    {"fast x1.03", 1.03, false, false, false, 0},
      {"stall, burst", 1.0, false, true, false, 0}, {"bot x2", 2.0, true, false, false, 0},
  };
  const sh::ModelData world = sh::build_brush_model({floor_box()});
  sh::Server sv;
  sh::create_server(sv, {}, {world.model()}, {});
  for (size_t i = 0; i < players.size(); i++)
    sh::connect_client(sv, static_cast<int>(i), false, players[i].bot);
  se::CheatWatch watch;
  se::cheat_watch_frame(watch, sv);
  const int frames = static_cast<int>(seconds / 0.013);
  const int stall_from = frames / 2, stall_to = stall_from + 154;  // two seconds
  std::vector<double> t_s;
  std::vector<std::vector<double>> score(players.size());
  std::vector<double> out_s(players.size(), -1);
  for (int f = 0; f < frames; f++) {
    sv.time += 0.013;
    for (size_t i = 0; i < players.size(); i++) {
      Player& p = players[i];
      p.owed_ms += 13 * p.rate;
      if (p.stalls && f >= stall_from && f < stall_to) continue;
      while (p.owed_ms >= 13) {  // whole commands of 13 ms on the client's own clock
        sh::MoveCmd c;
        c.msec = 13;
        sh::run_player_command(sv, static_cast<int>(i), c);
        p.owed_ms -= 13;
      }
    }
    se::cheat_watch_frame(watch, sv);
    if (f % 8 != 0) continue;
    t_s.push_back(sv.time);
    for (size_t i = 0; i < players.size(); i++) {
      score[i].push_back(watch.guard.clients[i].score);
      if (out_s[i] < 0 && se::cheat_watch_out(watch, static_cast<int>(i))) out_s[i] = sv.time;
    }
  }
  gl::LabTable table{
      "Each player: what the speed check cut, what the watch flagged, when it was out",
      {"player", "clock", "cut ms", "flags", "worst score", "out at s", "verdict"},
      {}};
  gl::LineChart chart{{"SPEED CHECK SCORE, EACH PLAYER", "", "t (s)", "score"},
                      {},
                      {},
                      {},
                      {},
                      false,
                      false,
                      false};
  int caught = 0, flagged_honest = 0;
  for (size_t i = 0; i < players.size(); i++) {
    const Player& p = players[i];
    const int slot = static_cast<int>(i);
    const gn::GuardClient& g = watch.guard.clients[i];
    const std::string verdict = se::cheat_watch_verdict(watch, slot);
    table.rows.push_back({p.who, "x" + fmt(p.rate, 2), std::to_string(sv.clients[i].speed_cut_ms),
                          std::to_string(g.counts[static_cast<size_t>(gn::Cheat::flood)]),
                          fmt(g.worst, 1), out_s[i] >= 0 ? fmt(out_s[i], 2) : "never",
                          verdict.empty() ? "-" : verdict});
    chart.traces.push_back({p.who, t_s, score[i]});
    std::string key = "se-cheats-";
    key += p.who;
    lab.metric(key + "-cut-ms", static_cast<double>(sv.clients[i].speed_cut_ms), "ms");
    lab.metric(key + "-out-s", out_s[i], "s");
    if (p.cheat && out_s[i] >= 0) caught++;
    if (p.cheat && out_s[i] < 0)
      lab.fail(std::string("the ") + p.who + " clock was not put out (worst score " +
                   fmt(g.worst, 1) + ")",
               "Read cheat_watch_frame in games/shooters_edge/net/qw_cheats.cpp and the cut "
               "in speed_check (plugins/shooter/client.cpp); skill: net.");
    if (!p.cheat && !verdict.empty()) {
      flagged_honest++;
      lab.fail(std::string("the ") + p.who + " player was put out: " + verdict,
               "Read kSpeedSlackMs in games/shooters_edge/net/qw_cheats.hpp and widen it only "
               "as far as a real clock explains; skill: net.");
    }
  }
  lab.metric("se-cheats-caught", caught, "count");
  lab.metric("se-cheats-honest-flagged", flagged_honest, "count");
  lab.table(std::move(table));
  lab.note(
      "clock: the client's time against the server's. cut ms: what the speed check took off its "
      "commands over the run (it lets a command run only as long as time passed plus a bank of "
      "at most 500 ms). flags: the seconds the watch found more than 50 ms cut. worst score: the "
      "highest the score reached (a flag adds 10, a second takes 1 away); past 40 the player is "
      "out and the match report names it. A bot is never checked.");
  chart.rules.push_back(gn::GuardRules{}.out_score);
  chart.frame.subtitle = "the out score dashed; the stall from " + fmt(seconds / 2, 0) +
                         " s, its burst two seconds later";
  const gl::Chart drawn = gl::line_chart(chart);
  lab.shot({"scores",
            drawn.title,
            "Each player's speed check score over the run; past the dashed line a player is out "
            "and its match report says so.",
            "both fast clocks on one line (each is cut more than 50 ms every second) crossing at "
            "the fifth flag, about six seconds in; the stalled player's one step at its burst "
            "decaying away; the honest, the 3% fast and the bot flat on zero.",
            {{"seconds", lab.arg("seconds")}}},
           gl::to_picture(drawn.plot));
}

const gl::LabSpec kSpec{
    "se-cheats",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/qw_cheats (the speed check's cuts scored)",
    "net",
    "Is a client whose clock runs fast put out, how soon, and is any honest player flagged?",
    {
        {"seconds", "30", "how long the run lasts"},
    },
    "--seconds=20",
    run,
};
GE_LAB(kSpec);

}  // namespace
