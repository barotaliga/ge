// LAB `se-own-ahead`: DOES THE JOINED WINDOW SHOW ITS OWN ROCKET AND HEAR ITS OWN JUMP AT ONCE?
// The arena's rules in C++ served by the QuakeWorld server on engine/net's simulated network, the
// window's client holding a rocket launcher: a press of attack, then a press of jump, every 1.2 s.
// For each one-way latency in `--latencies` the session runs twice: the player's own missiles and
// jump off (`cl_predict_projectiles 0`, `cl_predict_jump 0`: drawn and heard when the server's
// frame says so) and on (client/own_missiles.hpp; the jump off the predicted move). A table of the
// press to the first rocket drawn, the press to the jump heard, the rockets thrown ahead and how
// many the server's copy took over; a chart of one press at the first latency: how far the drawn
// rocket is from where it left, each window frame, off and on, and where the server's copy took
// over.
//
// ge:summary Lab: the joined window's own rocket drawn and jump heard ahead of the server
// ge:keywords lab se-own-ahead cl_predict_projectiles cl_predict_jump own_missiles handover
// ge:keywords predicted-projectiles jump-sound latency rocket
// ge:skill net labs
// ge:test tests/shooters_edge_own_missiles_test.cpp
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace sl = shooters_edge::labs;
namespace dc = shooters_edge::client;
using ge::core::num::to_fixed;

// One spawn point with a rocket launcher and rockets on it: the window joins holding it.
const char* const kEntities = R"({
"classname" "worldspawn"
"message" "own missiles and jump"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 0 24"
}
{
"classname" "weapon_rocketlauncher"
"origin" "-416 0 0"
}
{
"classname" "item_rockets"
"origin" "-416 0 0"
"spawnflags" "1"
}
{
"classname" "item_rockets"
"origin" "-400 0 0"
"spawnflags" "1"
}
)";

struct Session {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::QwServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  std::vector<sh::BrushModel> models;
  int64_t now = 0;
};

bool open_session(Session& s, double latency_ms, std::string& error) {
  if (!sv::baseline_mod(s.progs, error)) return false;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  s.qc = sv::make_qc_server(s.progs, so, error);
  if (!s.qc) return false;
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = kEntities;
  if (!sv::load_map(*s.qc, map, error)) return false;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(1, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortServer));
  gn::open_sim_transport(s.ct, s.net, gn::sim_address(1, cn::kPortClient));
  s.rules = sv::qc_runner_over(*s.qc);
  if (!cn::qw_start(s.server, s.rules, s.st, cn::ServerConfig{}, error)) return false;
  cn::ClientConfig wc;
  wc.name = "window";
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortServer), wc);
  s.models = sv::arena_models();
  return true;
}

void window_frame(Session& s, int64_t wall_us, int64_t window_us) {
  while (s.now + cn::kTickUs <= wall_us) {
    s.now += cn::kTickUs;
    gn::sim_advance(s.net, s.now);
    cn::qw_frame(s.server, s.now);
  }
  cn::remote_step(s.remote, static_cast<double>(window_us) * 1e-6, s.said);
}

std::string_view sample_of(const dc::DemoState& d, const dc::DemoSound& snd) {
  if (snd.name != nullptr) return snd.name;
  if (snd.sound > 0 && static_cast<size_t>(snd.sound) < d.sounds.size())
    return d.sounds[static_cast<size_t>(snd.sound)];
  return {};
}

// The rocket drawn nearest where it left (the player's own or the server's): its distance from
// `from`, or -1 when none is drawn.
double rocket_drawn(const dc::DemoState& d, const sh::Vec3f& from) {
  double best = -1;
  const auto take = [&](const dc::DemoEntity& e) {
    if (!e.present || e.model <= 0 || static_cast<size_t>(e.model) >= d.models.size()) return;
    if (d.models[static_cast<size_t>(e.model)] != "progs/missile.mdl") return;
    const double dist = sh::length(e.origin - from);
    if (best < 0 || dist < best) best = dist;
  };
  for (size_t n = d.players.size() + 1; n < d.entities.size(); n++) take(d.entities[n]);
  for (const dc::DemoEntity& e : d.own_missiles) take(e);
  return best;
}

struct Run {
  bool ok = false;
  bool agreed = false;
  double round_trip_ms = 0;
  int presses = 0, jumps = 0, thrown = 0, handed = 0;
  double first_rocket_ms = 0, first_jump_ms = 0;  // means over the presses
  std::vector<double> t, dist;                    // the chart's press
};

constexpr double kCycleS = 1.2, kFireS = 0.1, kJumpAtS = 0.6, kJumpS = 0.1;

Run run_session(double latency_ms, bool on, int fps, int presses) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, latency_ms, error)) return out;
  const int64_t window_us = 1000000 / fps;
  int64_t wall = 0;
  for (int i = 0; i < 6 * fps && s.remote.end.stage != cn::ClientStage::active; i++)
    window_frame(s, wall += window_us, window_us);
  for (int i = 0; i < fps; i++) window_frame(s, wall += window_us, window_us);  // picked up
  if (s.remote.end.stage != cn::ClientStage::active) return out;
  cn::remote_world(s.remote, &s.models);
  s.remote.missiles.on = on;
  s.remote.predict_jump = on;
  out.agreed = s.remote.end.ext.has(cn::ExtFamily::mvd, cn::kMvdWeaponPred);
  const dc::DemoState& d = s.remote.demo.state;
  const int cycle = static_cast<int>(kCycleS * fps);
  const int frames = cycle * presses;
  const int fire = static_cast<int>(kFireS * fps);
  const int jump_at = static_cast<int>(kJumpAtS * fps), jump_for = static_cast<int>(kJumpS * fps);
  int press_at = -1;
  bool rocket_this = false, jump_this = false;
  sh::Vec3f from;
  for (int i = 0; i < frames; i++) {
    const int in_cycle = i % cycle;
    if (in_cycle == 0) {
      press_at = i;
      rocket_this = jump_this = false;
      if (d.player >= 0 && static_cast<size_t>(d.player) < d.players.size())
        from = d.players[static_cast<size_t>(d.player)].origin;
      out.presses++;
    }
    s.remote.hands.buttons =
        (in_cycle < fire ? sh::kButtonAttack : 0) |
        (in_cycle >= jump_at && in_cycle < jump_at + jump_for ? sh::kButtonJump : 0);
    window_frame(s, wall += window_us, window_us);
    const double since_ms = static_cast<double>(i - press_at) * 1000.0 / fps;
    const double dist = rocket_drawn(d, from);
    if (!rocket_this && in_cycle < jump_at && dist >= 0) {
      rocket_this = true;
      out.first_rocket_ms += since_ms;
    }
    for (const dc::DemoSound& snd : d.started)
      if (!jump_this && in_cycle >= jump_at && snd.entity == d.player + 1 &&
          sample_of(d, snd) == "player/plyrjmp8.wav") {
        jump_this = true;
        out.jumps++;
        out.first_jump_ms += static_cast<double>(in_cycle - jump_at) * 1000.0 / fps;
      }
    if (i < cycle && in_cycle < jump_at) {
      out.t.push_back(since_ms);
      out.dist.push_back(dist);
    }
  }
  out.round_trip_ms = s.remote.end.latency_avg * 1000;
  out.first_rocket_ms /= out.presses;
  if (out.jumps > 0) out.first_jump_ms /= out.jumps;
  out.thrown = s.remote.missiles.thrown;
  out.handed = s.remote.missiles.handed;
  out.ok = true;
  return out;
}

void run(gl::Lab& lab) {
  const int fps = static_cast<int>(lab.arg_num("fps"));
  const int presses = static_cast<int>(lab.arg_num("presses"));
  std::vector<double> latencies;
  const std::string list = lab.arg("latencies");
  for (size_t at = 0; at < list.size();) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    latencies.push_back(std::stod(list.substr(at, comma - at)));
    at = comma + 1;
  }
  const std::vector<std::pair<std::string, std::string>> in = {{"fps", lab.arg("fps")},
                                                               {"presses", lab.arg("presses")},
                                                               {"latencies", lab.arg("latencies")}};
  if (fps < 30 || presses < 1 || latencies.empty()) {
    lab.fail(
        "se-own-ahead: --fps must be 30 or more, --presses 1 or more and --latencies name one or "
        "more",
        "next: make lab LAB=se-own-ahead ARGS=--help");
    return;
  }
  gl::LabTable t{
      "the window's own rocket and jump, a press of each every 1.2 s, off and on",
      {"one way ms", "round trip ms", "own missiles and jump", "press to the rocket drawn (ms)",
       "press to the jump heard (ms)", "rockets thrown ahead", "taken over by the server's"},
      {}};
  bool first = true;
  for (double ms : latencies) {
    Run runs[2];
    for (int k = 0; k < 2; k++) {
      runs[k] = run_session(ms, k == 1, fps, presses);
      const Run& r = runs[k];
      if (!r.ok || !r.agreed) {
        lab.fail("se-own-ahead: the window never reached play, or weapon prediction was not agreed",
                 "next: build/debug/tests/shooters_edge_own_missiles_test");
        return;
      }
      const std::string key = "ms-" + to_fixed(ms, 0) + (k == 1 ? ".on" : ".off");
      t.rows.push_back({to_fixed(ms, 0), to_fixed(r.round_trip_ms, 1), k == 1 ? "on" : "off",
                        to_fixed(r.first_rocket_ms, 1), to_fixed(r.first_jump_ms, 1),
                        std::to_string(r.thrown), std::to_string(r.handed)});
      lab.metric(key + ".rocket-drawn", r.first_rocket_ms, "ms");
      lab.metric(key + ".jump-heard", r.first_jump_ms, "ms");
      lab.metric(key + ".jumps-heard", r.jumps, "jumps");
      lab.metric(key + ".thrown", r.thrown, "rockets");
      lab.metric(key + ".handed", r.handed, "rockets");
    }
    if (runs[1].first_rocket_ms >= runs[0].first_rocket_ms)
      lab.fail("se-own-ahead: at " + to_fixed(ms, 0) +
                   " ms each way the own rocket was drawn no sooner than the server's",
               "next: build/debug/tests/shooters_edge_own_missiles_test");
    if (runs[1].jumps != runs[0].jumps || runs[1].first_jump_ms >= runs[0].first_jump_ms)
      lab.fail("se-own-ahead: at " + to_fixed(ms, 0) + " ms each way the jump was heard " +
                   std::to_string(runs[1].jumps) + " times after " +
                   to_fixed(runs[1].first_jump_ms, 1) + " ms against the server's " +
                   std::to_string(runs[0].jumps) + " after " + to_fixed(runs[0].first_jump_ms, 1),
               "next: build/debug/tests/shooter_predict_test");
    if (runs[1].thrown < runs[1].presses || runs[1].handed < runs[1].presses)
      lab.fail("se-own-ahead: at " + to_fixed(ms, 0) + " ms each way " +
                   std::to_string(runs[1].thrown) + " rockets were thrown ahead and " +
                   std::to_string(runs[1].handed) + " taken over for " +
                   std::to_string(runs[1].presses) + " presses",
               "next: build/debug/tests/shooters_edge_own_missiles_test");
    if (!first) continue;
    first = false;
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"ONE ROCKET, " + to_fixed(ms, 0) + " MS EACH WAY",
               "the window at " + std::to_string(fps) +
                   " fps: how far the drawn rocket is from where the player stood at the press",
               "since the press (ms)", "units"};
    for (int k = 0; k < 2; k++) {
      gl::Trace tr{k == 1 ? "own missiles on" : "own missiles off", {}, {}};
      for (size_t i = 0; i < runs[k].t.size(); i++)
        if (runs[k].dist[i] >= 0) {
          tr.x.push_back(runs[k].t[i]);
          tr.y.push_back(runs[k].dist[i]);
        }
      c.traces.push_back(tr);
    }
    sl::shoot(
        lab, "rocket-flight", gl::line_chart(c),
        "How far the drawn rocket is from where the player stood at the press, each window "
        "frame. Off, nothing is drawn until the server's rocket arrives a round trip later, "
        "already well on its way; on, the rocket leaves at the press and flies on smoothly, "
        "the server's copy, a round trip behind it on the same path, adopting it without a jump.",
        "the on line starts at the press and climbs in one straight line with no step; the "
        "off line starts a round trip later, a round trip's flight behind it",
        in);
  }
  lab.table(t);
}

const gl::LabSpec kSpec{
    "se-own-ahead",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client/own_missiles and net/qw_remote: the joined window's own missiles "
    "and jump shown ahead of the server",
    "net",
    "Does the window joined to a server draw its own rocket and hear its own jump at the press, "
    "and does the server's rocket take over without a jump?",
    {
        {"fps", "144", "the window's frames a second (the server runs 77)"},
        {"presses", "4", "presses measured a session"},
        {"latencies", "75,25",
         "one-way latencies, ms, a pair of sessions each (the first charted)"},
    },
    "--presses=2 --latencies=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
