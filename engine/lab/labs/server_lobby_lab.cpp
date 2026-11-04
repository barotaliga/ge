// LAB `server-lobby`: the engine's lobby before the match (engine/server/match.hpp) with no game
// under it. Players and bots take seats; each player says `ready` at a seeded moment, some take it
// back with `break` once; the countdown runs into the match, the match plays `--match` seconds and
// the map change brings warmup back, round after round. It answers how long each warmup and
// countdown takes, how many countdowns a break stops, whether the leader's `start` cuts a warmup
// short, and whether a session replays to the bit. The phases over time are drawn as a chart.
//
// ge:summary Lab: the engine's lobby on its own: warmup, ready and break, countdown, match, again
// ge:keywords lab server-lobby lobby match-phases warmup countdown ready break leader start
// ge:skill net labs
// ge:test tests/server_match_test.cpp
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "server/match.hpp"

namespace {

namespace gl = ge::lab;
namespace gs = ge::server;
namespace gc = ge::core;

constexpr double kStep = 0.05;  // the session's clock step, s

struct Round {
  double warmup_s = 0, countdown_s = 0;
};

struct Session {
  std::vector<double> t, phase;
  std::vector<Round> rounds;
  int countdowns = 0, stopped = 0, matches = 0;
  std::vector<std::string> said;
  uint64_t digest = 1469598103934665603ull;
};

// `players` people and `bots` bots seated at the start; each round every player readies at a
// seeded moment in its first `ready_by` seconds, and with `breaks` the first player takes it back
// once; with `leader_start` the leader starts the countdown halfway to `ready_by` whoever is ready.
Session run_session(uint32_t seed, int players, int bots, double seconds, double match_s,
                    double ready_by, bool breaks, bool leader_start) {
  Session s;
  gc::Rng rng = gc::make_rng(seed);
  gs::Lobby l;
  l.settings.min_players = 2;
  l.settings.countdown_s = 5;
  for (int i = 0; i < players; i++)
    gs::lobby_join(l, i, "player" + std::to_string(i + 1), false, 0);
  for (int i = 0; i < bots; i++)
    gs::lobby_join(l, players + i, "bot" + std::to_string(i + 1), true, 0);
  std::vector<double> ready_at(static_cast<size_t>(players));
  double round_start = 0, countdown_at = -1, match_at = -1;
  bool broke = false, started = false;
  const auto deal = [&](double from) {
    for (double& r : ready_at) r = from + gc::range(rng, 1, ready_by);
    broke = !breaks;
    started = !leader_start;
  };
  deal(0);
  const auto steps = static_cast<int64_t>(gc::num::round(seconds / kStep));
  gs::LobbyPhase last = l.phase;
  for (int64_t k = 0; k <= steps; k++) {
    const double now = static_cast<double>(k) * kStep;
    if (l.phase != gs::LobbyPhase::match) {
      for (int i = 0; i < players; i++) {
        const double at = ready_at[static_cast<size_t>(i)];
        if (at >= 0 && now >= at) {
          gs::lobby_command(l, i, "ready", now);
          ready_at[static_cast<size_t>(i)] = -1;
        }
      }
      if (!broke && l.phase == gs::LobbyPhase::countdown && countdown_at >= 0 &&
          now >= countdown_at + 1) {
        gs::lobby_command(l, 0, "break", now);
        ready_at[0] = now + 2;
        broke = true;
      }
      if (!started && now >= round_start + ready_by / 2) {
        gs::lobby_command(l, 0, "start", now);
        started = true;
      }
    }
    gs::lobby_due(l, now);
    if (l.phase == gs::LobbyPhase::match && match_at >= 0 && now >= match_at + match_s) {
      gs::lobby_back(l);  // the match's map change
      round_start = now;
      countdown_at = match_at = -1;
      deal(now);
    }
    if (l.phase != last) {
      if (l.phase == gs::LobbyPhase::countdown) countdown_at = now;
      if (l.phase == gs::LobbyPhase::match) {
        match_at = now;
        s.rounds.push_back({countdown_at - round_start, now - countdown_at});
      }
      if (!s.t.empty()) {  // a step, not a ramp
        s.t.push_back(now);
        s.phase.push_back(static_cast<double>(last));
      }
      last = l.phase;
    }
    if (s.t.empty() || l.phase != static_cast<gs::LobbyPhase>(s.phase.back())) {
      s.t.push_back(now);
      s.phase.push_back(static_cast<double>(l.phase));
    }
    s.digest = (s.digest ^ static_cast<uint64_t>(l.phase)) * 1099511628211ull;
  }
  s.t.push_back(seconds);
  s.phase.push_back(static_cast<double>(l.phase));
  s.countdowns = l.countdowns;
  s.stopped = l.stopped;
  s.matches = l.matches;
  s.said = l.said;
  for (const std::string& line : l.said)
    for (char ch : line) s.digest = (s.digest ^ static_cast<uint8_t>(ch)) * 1099511628211ull;
  return s;
}

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

std::string secs(double s) { return gc::num::to_fixed(s, 2); }

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const int players = static_cast<int>(lab.arg_num("players"));
  const int bots = static_cast<int>(lab.arg_num("bots"));
  const double seconds = lab.arg_num("seconds");
  const double match_s = lab.arg_num("match");
  if (players < 1 || players + bots > gs::kLobbySeats) {
    lab.fail("--players=" + lab.arg("players") + " --bots=" + lab.arg("bots") +
                 " do not fit the lobby's seats",
             "Pick at least one player and at most " + std::to_string(gs::kLobbySeats) +
                 " seats in all; skill: net.");
    return;
  }
  struct Case {
    const char* name;
    bool breaks, leader_start;
  };
  const Case cases[] = {{"ready", false, false}, {"break", true, false}, {"start", false, true}};
  gl::LabTable rounds{"each round: the warmup until the countdown, and the countdown (s)",
                      {"case", "round", "warmup", "countdown"},
                      {}};
  gl::LabTable runs{"each case: what the lobby counted, and the session run twice",
                    {"case", "countdowns", "stopped", "matches", "digest", "again", "agree"},
                    {}};
  gl::LineChart line;
  line.frame = {"THE LOBBY'S PHASES", "seed " + lab.arg("seed"), "t (s)",
                "phase (0 warmup, 1 countdown, 2 match)"};
  line.spikes = false;
  int disagree = 0;
  for (const Case& c : cases) {
    const Session a =
        run_session(seed, players, bots, seconds, match_s, 12, c.breaks, c.leader_start);
    const Session b =
        run_session(seed, players, bots, seconds, match_s, 12, c.breaks, c.leader_start);
    for (size_t i = 0; i < a.rounds.size(); i++)
      rounds.rows.push_back({c.name, std::to_string(i + 1), secs(a.rounds[i].warmup_s),
                             secs(a.rounds[i].countdown_s)});
    const bool agree = a.digest == b.digest;
    disagree += agree ? 0 : 1;
    runs.rows.push_back({c.name, std::to_string(a.countdowns), std::to_string(a.stopped),
                         std::to_string(a.matches), hex(a.digest), hex(b.digest),
                         agree ? "yes" : "NO"});
    line.traces.push_back({c.name, a.t, a.phase});
    lab.metric(std::string(c.name) + ".matches", a.matches, "matches");
    lab.metric(std::string(c.name) + ".stopped", a.stopped, "countdowns");
    if (std::string(c.name) == "break" && !a.said.empty())
      lab.note("first line: " + a.said.front());
  }
  lab.metric("disagreeing_cases", disagree, "cases");
  lab.table(std::move(rounds));
  lab.table(std::move(runs));
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"phases",
            chart.title,
            "The engine's lobby with no game under it: players and bots seated, each player "
            "ready at a seeded moment; `break` has the first player take it back once, `start` "
            "has the leader start the countdown early. A match lasts --match seconds, then warmup "
            "again.",
            "every case reaches the match each round; break's first countdown stopped; start's "
            "warmups cut to 6 s",
            {{"players", lab.arg("players")}, {"bots", lab.arg("bots")}}},
           gl::to_picture(chart.plot));
}

const gl::LabSpec kSpec{
    "server-lobby",
    gl::LabKind::sim,
    "engine",
    "engine/server/match (the lobby before the match, its phases)",
    "net",
    "Does the engine's lobby take every round from warmup through the countdown into the match and "
    "back, stop a countdown on a break, let the leader start early, and replay to the bit?",
    {
        {"players", "3", "people seated, each ready at a seeded moment"},
        {"bots", "3", "bots seated, ready as they join"},
        {"seconds", "120", "the session's length, s"},
        {"match", "30", "each match's length before the map changes, s"},
        {"seed", "1", "the ready moments' stream"},
    },
    "--seconds=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
