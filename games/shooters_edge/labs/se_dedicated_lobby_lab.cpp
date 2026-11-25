// LAB `se-dedicated-lobby`: the engine's lobby on Shooter's Edge's dedicated server
// (net/qw_game.hpp, `sv_lobby 1`), with `--bots` bots seated and two classic clients joining
// over the simulated network. Every map opens in warmup with the limits held; each client says
// `ready` a few seconds into each warmup, the countdown runs into the match (the map restarted with
// every seat on it), the match plays to its `--match` seconds, the mod changes the map and warmup
// comes back, round after round; with `--intermission` the scores stay up that long after the
// mod's end of each match before the next map (`sv_intermission`). It answers how long each warmup
// and countdown take, whether the bots play as seats every client sees (their teams and frags on a
// client's scoreboard), and whether a session replays to the bit. The phases over time are drawn as
// a chart. The core reads each phase from the game (ServerGame::phase) and records each match by
// itself (sv_autorecord's recorder): each played back against the live server's views.
//
// ge:summary Lab: the lobby on the dedicated server: bots seated, clients ready, warmup to match
// ge:keywords lab se-dedicated-lobby lobby sv_lobby sv_bots bots warmup countdown match ready
// ge:keywords scoreboard local-seat match-restart sv_countdown sv_intermission intermission
// ge:skill net labs
// ge:test tests/shooters_edge_qw_game_test.cpp
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "console/cvar.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/sim_network.hpp"
#include "server/autorecord.hpp"
#include "server/match.hpp"
#include "server/playback.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_game.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;
namespace gc = ge::core;

constexpr uint16_t kPort = 27500, kClientPort = 27001;
constexpr int kClients = 2;

struct Round {
  double warmup_s = 0, countdown_s = 0;
  double intermission_s = -1;  // the intermission after this round's match (-1: none seen)
};

struct Session {
  std::string error;
  std::vector<double> t, phase;
  std::vector<double> ft;                  // the frags' sample times, s
  std::vector<std::vector<double>> frags;  // a seat's frags on player1's board, by sample
  std::vector<std::string> seat_names;
  std::vector<Round> rounds;
  std::vector<std::vector<std::string>> board;  // the first client's scoreboard at the end
  std::vector<std::string> lines;               // the lobby's lines the first client printed
  std::string status;
  int matches = 0;
  uint64_t digest = 1469598103934665603ull;
  // The core's view of the phases (ServerGame::phase) and every match recorded by itself
  // (sv_autorecord's recorder): each match's length, bytes and views played back that differ.
  int phase_disagrees = 0;  // frames the core's phase and the status line's differ
  struct Kept {
    double seconds = 0;
    size_t bytes = 0;
    int64_t frames = 0, views = 0, wrong = 0;
  };
  std::vector<Kept> kept;
};

// The phase the server's status names: 0 warmup, 1 countdown, 2 match, 3 intermission (-1: none).
int phase_of(const std::string& status) {
  for (int p = 0; p < 4; p++)
    if (status.find(std::string("lobby=") + gs::phase_name(static_cast<gs::LobbyPhase>(p))) !=
        std::string::npos)
      return p;
  return -1;
}

Session run_session(uint32_t seed, int bots, double seconds, double match_s, double countdown_s,
                    double intermission_s, const std::string& modern) {
  Session r;
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(seed, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  gs::ServerConfig c;
  // The server's console over the config, so `sv_modern` sets what it means for this game.
  ge::console::Cvars cvars;
  gs::add_server_cvars(cvars, c, se::qw_game());
  cvars.set("sv_modern", modern, ge::console::Source::local);
  c.hostname = "Lobby Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = seed;
  c.bots = bots;
  c.lobby = 1;
  c.countdown_s = countdown_s;
  c.intermission_s = intermission_s;
  c.rules["timelimit"] = gc::num::to_fixed(match_s / 60, 4);
  gs::Server s;
  if (!gs::server_open(s, se::qw_game(), c, &port, nullptr, r.error)) return r;
  gn::Transport ct[kClients];
  se::QwClientEnd ends[kClients];
  double ready_at[kClients] = {-1, -1};
  for (int i = 0; i < kClients; i++)
    gn::open_sim_transport(ct[i], net, gn::sim_address(static_cast<uint32_t>(i + 1), kClientPort));
  const auto frames = static_cast<int64_t>(gc::num::round(seconds / s.frame_s));
  gs::MatchRecorder rec;
  std::vector<std::vector<std::vector<uint8_t>>> live;  // the match's views by frame
  const auto views = [&] {
    std::vector<std::vector<uint8_t>> row(8);
    for (int p = 0; p < 8; p++) s.game->player_view(s.state, p, row[static_cast<size_t>(p)]);
    live.push_back(std::move(row));
  };
  const auto keep = [&] {
    Session::Kept k;
    k.bytes = rec.rec.bytes.size();
    k.seconds = static_cast<double>(s.now_us - rec.started_us) / 1e6;
    gs::Playback pb;
    std::string e;
    if (gs::playback_open(pb, *s.game, rec.rec.bytes, e)) {
      k.frames = static_cast<int64_t>(pb.frames.size());
      const auto same = [&](int64_t at) {
        for (int p = 0; p < 8; p++) {
          std::vector<uint8_t> v;
          gs::playback_view(pb, p, v);
          k.views++;
          const int bad = static_cast<size_t>(at) < live.size() &&
                                  v == live[static_cast<size_t>(at)][static_cast<size_t>(p)]
                              ? 0
                              : 1;
          k.wrong += bad;
        }
      };
      same(0);
      while (gs::playback_step(pb, e)) same(pb.at);
    } else {
      k.wrong = -1;
    }
    r.kept.push_back(k);
    live.clear();
  };
  int last = -1;
  double warm_at = 0, count_at = 0, over_at = 0;
  for (int64_t f = 0; f <= frames; f++) {
    const int64_t now = s.now_us;
    const double t = static_cast<double>(now) / 1e6;
    gn::sim_advance(net, now);
    for (int i = 0; i < kClients; i++) {
      if (now < static_cast<int64_t>(i + 1) * 1000000) continue;
      if (ends[i].transport == nullptr) {
        se::ClientConfig cc;
        cc.name = "player" + std::to_string(i + 1);
        cc.files = {"skins/base.pcx"};
        cc.seed = i + 1;
        cc.qport = static_cast<uint16_t>(1000 + i);
        se::client_start(ends[i], ct[i], gn::sim_address(0, kPort), cc, t);
      }
      if (ready_at[i] >= 0 && t >= ready_at[i] && ends[i].stage == se::ClientStage::active) {
        se::client_command(ends[i], "ready");
        ready_at[i] = -1;
      }
      se::client_tick(ends[i], now);
    }
    gs::server_frame(s);
    r.digest = (r.digest ^ gs::server_digest(s)) * 1099511628211ull;
    if (f % 39 == 0) {  // every half second
      if (r.frags.empty()) r.frags.resize(8);
      r.ft.push_back(t);
      for (size_t k = 0; k < 8; k++) r.frags[k].push_back(ends[0].frags[k]);
    }
    const int p = phase_of(gs::server_status(s));
    r.phase_disagrees += p != static_cast<int>(s.phase) ? 1 : 0;
    std::string rec_error;
    switch (gs::match_record_frame(rec, s, rec_error)) {
      case gs::MatchRecord::started:
        live.clear();
        views();
        break;
      case gs::MatchRecord::recording:
        views();
        break;
      case gs::MatchRecord::finished:
        keep();
        break;
      case gs::MatchRecord::failed:
        r.error = "the match's recording did not start: " + rec_error;
        return r;
      case gs::MatchRecord::none:
        break;
    }
    if (p != last) {
      if (p == 0) {  // a warmup: each client ready a few seconds in
        warm_at = t;
        for (int i = 0; i < kClients; i++) ready_at[i] = t + 3 + 2 * i;
      }
      if (p == 1) count_at = t;
      if (p == 2 && last == 1) {
        r.rounds.push_back({count_at - warm_at, t - count_at});
        r.matches++;
      }
      if (p == 3) over_at = t;
      if (last == 3 && !r.rounds.empty()) r.rounds.back().intermission_s = t - over_at;
      if (last >= 0) {  // a step, not a ramp
        r.t.push_back(t);
        r.phase.push_back(last);
      }
      r.t.push_back(t);
      r.phase.push_back(p);
      last = p;
    }
  }
  r.t.push_back(seconds);
  r.phase.push_back(last);
  const se::QwClientEnd& me = ends[0];
  r.seat_names.resize(8);
  for (int i = 0; i < 32; i++) {
    const std::string& info = me.userinfos[i];
    if (info.empty()) continue;
    const std::string name = se::info_get(info, "name");
    if (name.empty()) continue;
    if (i < 8) r.seat_names[static_cast<size_t>(i)] = name;
    r.board.push_back({std::to_string(i), name, se::info_get(info, "team"),
                       se::info_get(info, "*bot") == "1" ? "bot" : "player",
                       std::to_string(me.frags[i])});
    r.digest = (r.digest ^ static_cast<uint64_t>(me.frags[i] + 1000)) * 1099511628211ull;
  }
  for (const std::string& line : me.prints)
    if (line.find("ready") != std::string::npos || line.find("countdown") != std::string::npos ||
        line.find("joins team") != std::string::npos || line.find("match") != std::string::npos ||
        line.find("next map") != std::string::npos)
      r.lines.push_back(line.substr(0, line.find('\n')));
  r.status = gs::server_status(s);
  return r;
}

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const int bots = static_cast<int>(lab.arg_num("bots"));
  const double seconds = lab.arg_num("seconds");
  const double match_s = lab.arg_num("match");
  const double countdown_s = lab.arg_num("countdown");
  const double intermission_s = lab.arg_num("intermission");
  const std::string modern = lab.arg("modern");
  if (bots < 0 || bots > 5) {
    lab.fail("--bots=" + lab.arg("bots") + " does not leave the two clients their seats",
             "Pick 0 to 5 bots (the lab's server has 8 slots, slot 0 kept for a player); "
             "skill: net.");
    return;
  }
  const Session a = run_session(seed, bots, seconds, match_s, countdown_s, intermission_s, modern);
  const Session b = run_session(seed, bots, seconds, match_s, countdown_s, intermission_s, modern);
  if (!a.error.empty()) {
    lab.fail("the dedicated server did not open: " + a.error,
             "Fix what the error names in games/shooters_edge/net/qw_game.cpp's create, then "
             "run the lab again; skill: net.");
    return;
  }
  gl::LabTable rounds{"each round: the warmup until the countdown, and the countdown (s)",
                      {"round", "warmup", "countdown"},
                      {}};
  if (intermission_s > 0) rounds.columns.push_back("intermission");
  for (size_t i = 0; i < a.rounds.size(); i++) {
    std::vector<std::string> row{std::to_string(i + 1), gc::num::to_fixed(a.rounds[i].warmup_s, 2),
                                 gc::num::to_fixed(a.rounds[i].countdown_s, 2)};
    if (intermission_s > 0)
      row.push_back(
          a.rounds[i].intermission_s < 0 ? "-" : gc::num::to_fixed(a.rounds[i].intermission_s, 2));
    rounds.rows.push_back(std::move(row));
  }
  gl::LabTable board{"player1's scoreboard at the end: every seat, its team and frags",
                     {"slot", "name", "team", "kind", "frags"},
                     a.board};
  gl::LabTable said{"the lobby's lines player1 was told, in order", {"line"}, {}};
  for (size_t i = 0; i < a.lines.size() && i < 16; i++) said.rows.push_back({a.lines[i]});
  const bool agree = a.digest == b.digest;
  gl::LabTable runs{
      "the session run twice",
      {"matches", "digest", "again", "agree"},
      {{std::to_string(a.matches), hex(a.digest), hex(b.digest), agree ? "yes" : "NO"}}};
  int seen_bots = 0;
  for (const auto& row : a.board) seen_bots += row[3] == "bot" ? 1 : 0;
  lab.metric("matches", a.matches, "matches");
  lab.metric("bots_on_the_scoreboard", seen_bots, "bots");
  lab.metric("disagreeing_runs", agree ? 0 : 1, "runs");
  // Every match kept by itself (sv_autorecord's recorder on the core's phase).
  gl::LabTable kept{
      "each match recorded by itself (sv_autorecord): its length, bytes and views "
      "played back against the live server's",
      {"match", "seconds", "frames", "bytes", "views", "views wrong"},
      {}};
  int64_t kept_wrong = 0;
  for (size_t i = 0; i < a.kept.size(); i++) {
    const Session::Kept& k = a.kept[i];
    kept_wrong += k.wrong < 0 ? 1 : k.wrong;
    kept.rows.push_back({std::to_string(i + 1), gc::num::to_fixed(k.seconds, 2),
                         std::to_string(k.frames), std::to_string(k.bytes), std::to_string(k.views),
                         k.wrong < 0 ? "did not open" : std::to_string(k.wrong)});
  }
  lab.metric("matches_recorded", static_cast<double>(a.kept.size()), "matches");
  lab.metric("recorded_views_wrong", static_cast<double>(kept_wrong), "views");
  lab.metric("phase_disagreements", a.phase_disagrees, "frames");
  lab.table(std::move(kept));
  if (kept_wrong != 0 || a.phase_disagrees != 0 || static_cast<int>(a.kept.size()) + 1 < a.matches)
    lab.fail(std::to_string(a.kept.size()) + " of " + std::to_string(a.matches) +
                 " matches recorded by themselves, " + std::to_string(kept_wrong) +
                 " views played back wrong, " + std::to_string(a.phase_disagrees) +
                 " frames the core's phase differs from the lobby's",
             "next: run make test T=server_autorecord and read qw_game.cpp's phase hook; "
             "skill: replay.");
  lab.table(std::move(rounds));
  lab.table(std::move(board));
  lab.table(std::move(said));
  lab.table(std::move(runs));
  lab.note("last status: " + a.status);
  gl::LineChart line;
  line.frame = {"THE DEDICATED SERVER'S LOBBY", "seed " + lab.arg("seed"), "t (s)",
                "phase (0 warmup, 1 countdown, 2 match, 3 intermission)"};
  line.spikes = false;
  line.traces.push_back({"lobby", a.t, a.phase});
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"phases",
            chart.title,
            "Shooter's Edge's dedicated server with sv_lobby 1 and bots seated: two classic "
            "clients join at 1 s and 2 s over the simulated network and say ready 3 s and 5 s "
            "into every warmup. The countdown runs --countdown seconds into the match (the map "
            "restarted with every seat on it); the match lasts --match seconds, the mod's "
            "intermission, then --intermission seconds more with the scores up (sv_intermission, "
            "phase 3; none at 0), then warmup again on the next map.",
            "warmup until both are ready, the countdown, the match, the intermission when set, "
            "warmup again, each round",
            {{"bots", lab.arg("bots")},
             {"match", lab.arg("match")},
             {"countdown", lab.arg("countdown")},
             {"intermission", lab.arg("intermission")}}},
           gl::to_picture(chart.plot));
  gl::LineChart fl;
  fl.frame = {"FRAGS ON PLAYER1'S SCOREBOARD", "seed " + lab.arg("seed"), "t (s)", "frags"};
  fl.spikes = false;
  for (size_t k = 0; k < a.frags.size() && k < a.seat_names.size(); k++)
    if (!a.seat_names[k].empty()) fl.traces.push_back({a.seat_names[k], a.ft, a.frags[k]});
  const gl::Chart fc = gl::line_chart(fl);
  lab.shot({"frags",
            fc.title,
            "Each seat's frags as the first client's scoreboard shows them, sampled every half "
            "second: the bots fight in warmup and in the match, the two idle clients mostly die. "
            "Every map change (the match's restart, the mod's next map) sets the scores back to "
            "0.",
            "the bots' frags climb, drop to 0 at each restart, climb again; the idle players "
            "near 0",
            {{"bots", lab.arg("bots")}, {"match", lab.arg("match")}}},
           gl::to_picture(fc.plot));
}

const gl::LabSpec kSpec{
    "se-dedicated-lobby",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/qw_game (the lobby and bots on the dedicated server)",
    "net",
    "Does the dedicated server's lobby take every round from warmup through the countdown into "
    "the match and back, with its bots as seats every client sees, and replay to the bit; does "
    "the core see each phase, and is each match recorded by itself as it was played?",
    {
        {"bots", "3", "bots seated at the start (sv_bots)"},
        {"countdown", "5", "everyone ready to the match, s (sv_countdown)"},
        {"intermission", "0",
         "the scores up after each match before the next map, s (sv_intermission)"},
        {"match", "15", "each match's length (the mod's timelimit), s"},
        {"modern", "0", "sv_modern: 1 every shot and projectile rewound, as a modern server"},
        {"seconds", "60", "server time to run, s"},
        {"seed", "1", "the network's and the game's stream"},
    },
    "--seconds=20",
    run,
};
GE_LAB(kSpec);

}  // namespace
