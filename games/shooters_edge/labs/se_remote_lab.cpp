// LAB `se-remote`: how smoothly, and how near the server's place, does the window joined to a
// server (games/shooters_edge/net/classic_remote.hpp) draw the other players and the missiles?
// The test arena served by the classic server on engine/net's simulated network, the window's
// client beside a scripted player who runs and turns in circles and fires now and then; the window
// draws at `--fps` while the server runs its 77 frames a second. For each one-way latency in
// `--latencies`, the session runs twice, the other players' prediction and the entities' lerp off
// (`cl_predict_players 0`, `cl_nolerp 1`) and on: a table of how often the other player moved
// between two window frames, how far his drawn place sat from the server's at that moment, how far
// ahead he was run; how often a missile was drawn between two frames; a chart of his x over a
// second (the server's, as framed, drawn off and on) and his path from above, at the first
// latency. Past a 20 ms round trip the drawn time falls before the newest record, so the other
// players are drawn as the frame put them (the classic client's rule): the table shows it.
//
// ge:summary Lab: the joined window's other players run ahead and missiles lerped, off and on
// ge:keywords lab se-remote remote-client predict_other other-players interpolation lerp
// ge:keywords cl_predict_players cl_nolerp smoothness latency window-fps
// ge:skill net labs
// ge:test tests/shooters_edge_remote_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;

// The arena served, the window's client and a scripted player on one simulated LAN.
struct Session {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct, bt;
  cn::ClassicServer server;
  cn::Remote remote;
  cn::ClassicClientEnd other;
  std::vector<std::string> said;
  std::vector<sh::BrushModel> models;
  int64_t now = 0;
};

bool open_session(Session& s, double latency_ms, std::string& error) {
  if (!sv::build_test_mod(s.progs, error)) return false;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  s.qc = sv::make_qc_server(s.progs, so, error);
  if (!s.qc) return false;
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  if (!sv::load_map(*s.qc, map, error)) return false;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(1, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortServer));
  gn::open_sim_transport(s.ct, s.net, gn::sim_address(1, cn::kPortClient));
  gn::open_sim_transport(s.bt, s.net, gn::sim_address(2, cn::kPortClient));
  s.rules = sv::qc_runner_over(*s.qc);
  if (!cn::classic_start(s.server, s.rules, s.st, cn::ServerConfig{}, error)) return false;
  cn::ClientConfig wc;
  wc.name = "window";
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortServer), wc);
  cn::ClientConfig oc;
  oc.name = "runner";
  cn::client_start(s.other, s.bt, gn::sim_address(0, cn::kPortServer), oc, 0);
  s.models = sv::arena_models();
  return true;
}

// The server's frames due by `wall_us`, then one window frame of `window_us`.
void window_frame(Session& s, int64_t wall_us, int64_t window_us) {
  while (s.now + cn::kTickUs <= wall_us) {
    s.now += cn::kTickUs;
    gn::sim_advance(s.net, s.now);
    cn::classic_frame(s.server, s.now);
    cn::client_tick(s.other, s.now);
  }
  cn::remote_step(s.remote, static_cast<double>(window_us) * 1e-6, s.said);
}

int other_slot(const Session& s) {
  for (int i = 0; i < 32; i++)
    if (i != s.remote.end.playernum && s.remote.end.player_seen[i] &&
        s.remote.end.players[i].has_command)
      return i;
  return -1;
}

struct Run {
  bool ok = false;
  int frames = 0, moved = 0, predicted = 0, lerped = 0, most_msec = 0;
  double drawn_err = 0, framed_err = 0, worst_err = 0;  // units, to the server's place
  std::vector<double> t, server_x, framed_x, drawn_x;   // the chart's second
  std::vector<sh::Vec3f> path;                          // drawn, the first quarter second
};

Run run_session(double latency_ms, bool on, int fps, double seconds) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, latency_ms, error)) return out;
  const int64_t window_us = 1000000 / fps;
  int64_t wall = 0;
  // Both sign on and play a second before anything is measured.
  for (int i = 0; i < 6 * fps && other_slot(s) < 0; i++)
    window_frame(s, wall += window_us, window_us);
  for (int i = 0; i < fps; i++) window_frame(s, wall += window_us, window_us);
  const int slot = other_slot(s);
  if (slot < 0 || s.remote.end.stage != cn::ClientStage::active) return out;
  cn::remote_world(s.remote, &s.models);
  s.remote.predict_players = on;
  s.remote.nolerp = !on;
  const auto at = static_cast<size_t>(slot);
  sh::Vec3f last = s.remote.demo.state.players[at].origin;
  const int frames = static_cast<int>(seconds * fps);
  for (int i = 0; i < frames; i++) {
    s.remote.hands.buttons = i % (fps / 2) < 2 ? sh::kButtonAttack : 0;  // a missile to lerp
    window_frame(s, wall += window_us, window_us);
    const sh::Vec3f drawn = s.remote.demo.state.players[at].origin;
    const sh::Vec3f framed = s.remote.end.players[slot].origin;
    const sh::NetPlayer n = sh::net_player_of(s.qc->sv, slot);
    const sh::Vec3f truth{static_cast<float>(n.origin8[0]) / 8.0f,
                          static_cast<float>(n.origin8[1]) / 8.0f,
                          static_cast<float>(n.origin8[2]) / 8.0f};
    out.frames++;
    if (sh::length(drawn - last) > 0) out.moved++;
    if (s.remote.others > 0) out.predicted++;
    if (s.remote.lerped > 0) out.lerped++;
    out.most_msec = std::max(out.most_msec, s.remote.others_msec);
    const double e = sh::length(drawn - truth);
    out.drawn_err += e;
    out.worst_err = std::max(out.worst_err, e);
    out.framed_err += sh::length(framed - truth);
    if (i < fps / 8) {
      out.t.push_back(static_cast<double>(i) * 1000.0 / fps);
      out.server_x.push_back(truth.x);
      out.framed_x.push_back(framed.x);
      out.drawn_x.push_back(drawn.x);
    }
    if (i < fps / 4) out.path.push_back(drawn);
    if (i >= fps / 8 && i < fps / 4) out.t.push_back(static_cast<double>(i) * 1000.0 / fps);
    last = drawn;
  }
  out.drawn_err /= out.frames;
  out.framed_err /= out.frames;
  out.ok = true;
  return out;
}

void run(gl::Lab& lab) {
  const int fps = static_cast<int>(lab.arg_num("fps"));
  const double seconds = lab.arg_num("seconds");
  std::vector<double> latencies;
  const std::string list = lab.arg("latencies");
  for (size_t at = 0; at < list.size();) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    latencies.push_back(std::stod(list.substr(at, comma - at)));
    at = comma + 1;
  }
  const std::vector<std::pair<std::string, std::string>> in = {{"fps", lab.arg("fps")},
                                                               {"seconds", lab.arg("seconds")},
                                                               {"latencies", lab.arg("latencies")}};
  if (fps < 30 || latencies.empty()) {
    lab.fail("se-remote: --fps must be 30 or more and --latencies name one or more",
             "next: make lab LAB=se-remote ARGS=--help");
    return;
  }
  gl::LabTable t{"the other player and the missiles as the window draws them, off and on",
                 {"one way ms", "prediction and lerp", "window frames", "he moved %", "run ahead %",
                  "most ms ahead", "drawn from server's (mean)", "worst",
                  "framed from server's (mean)", "frames with a missile lerped %"},
                 {}};
  bool first = true;
  for (double ms : latencies) {
    Run runs[2];
    for (int k = 0; k < 2; k++) {
      runs[k] = run_session(ms, k == 1, fps, seconds);
      if (!runs[k].ok) {
        lab.fail("se-remote: the window or the scripted player never reached play",
                 "next: build/debug/tests/shooters_edge_remote_test");
        return;
      }
      const Run& r = runs[k];
      const std::string key = "ms-" + to_fixed(ms, 0) + (k == 1 ? ".on" : ".off");
      const double pct = 100.0 / r.frames;
      t.rows.push_back({to_fixed(ms, 0), k == 1 ? "on" : "off", std::to_string(r.frames),
                        to_fixed(r.moved * pct, 1), to_fixed(r.predicted * pct, 1),
                        std::to_string(r.most_msec), to_fixed(r.drawn_err, 2),
                        to_fixed(r.worst_err, 2), to_fixed(r.framed_err, 2),
                        to_fixed(r.lerped * pct, 1)});
      lab.metric(key + ".moved", r.moved * pct, "%");
      lab.metric(key + ".drawn-error", r.drawn_err, "units");
      lab.metric(key + ".framed-error", r.framed_err, "units");
      lab.metric(key + ".lerped", r.lerped * pct, "%");
    }
    // The drawn time is the window's less the round trip plus 0.02 s, so past a 20 ms round trip
    // it falls before the newest record and the other players are drawn as the frame put them:
    // only the first (lowest) latency is held to moving more often.
    if (!first)
      lab.note("at " + to_fixed(ms, 0) +
               " ms each way the round trip passes 20 ms: the other "
               "players are drawn at the newest record, as the classic client draws them");
    if (first && runs[1].moved <= runs[0].moved)
      lab.fail("se-remote: at " + to_fixed(ms, 0) +
                   " ms the other player moved no more often with prediction on",
               "next: build/debug/tests/shooters_edge_remote_test");
    if (!first) continue;
    first = false;
    gl::LineChart c;
    c.spikes = false;
    c.points = true;
    c.frame = {"THE OTHER PLAYER'S X OVER AN EIGHTH OF A SECOND",
               to_fixed(ms, 0) + " ms each way, the window at " + std::to_string(fps) +
                   " fps, the server at 77",
               "window time (ms)", "x (units)"};
    c.traces.push_back({"the server's place", runs[1].t, runs[1].server_x});
    c.traces.push_back({"as the newest frame put him", runs[1].t, runs[1].framed_x});
    c.traces.push_back({"drawn, prediction off", runs[0].t, runs[0].drawn_x});
    c.traces.push_back({"drawn, prediction on", runs[1].t, runs[1].drawn_x});
    sl::shoot(
        lab, "other-x", gl::line_chart(c),
        "The scripted player's x, a sample each window frame for the first eighth of a second "
        "measured. "
        "Off, the window draws him where the newest frame put him: a staircase that steps "
        "when a frame arrives and holds flat between. On, he is run ahead from his last "
        "command every window frame, so the line climbs between the server's frames.",
        "the off line a staircase; the on line smooth and nearer the server's line than "
        "the framed one",
        in);
    gl::LineChart step;
    step.spikes = false;
    step.frame = {"HOW FAR HE MOVED EACH WINDOW FRAME",
                  "the first quarter second measured, off and on", "window time (ms)",
                  "units moved since the last window frame"};
    for (int k = 0; k < 2; k++) {
      gl::Trace tr{k == 1 ? "prediction on" : "prediction off", {}, {}};
      const int n = std::min(static_cast<int>(runs[k].path.size()), fps / 4);
      for (int i = 1; i < n; i++) {
        tr.x.push_back(runs[k].t[static_cast<size_t>(i)]);
        tr.y.push_back(sh::length(runs[k].path[static_cast<size_t>(i)] -
                                  runs[k].path[static_cast<size_t>(i) - 1]));
      }
      step.traces.push_back(tr);
    }
    sl::shoot(
        lab, "other-steps", gl::line_chart(step),
        "How far the scripted player's drawn place moved between two window frames. Off, "
        "he stands still for two window frames in three and then jumps a whole server "
        "frame's run; on, he moves on most window frames (four in five at 5 ms each way).",
        "off: a comb of zeros and tall teeth; on: lower teeth with steps between them, touching "
        "zero far less often",
        in);
  }
  lab.table(t);
}

const gl::LabSpec kSpec{
    "se-remote",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_remote: the window joined to a server, drawing the other "
    "players run ahead and the missiles between frames",
    "net",
    "How smoothly, and how near the server's place, does the window joined to a server draw the "
    "other players and the missiles, with their prediction and lerp off and on?",
    {
        {"fps", "240", "the window's frames a second (the server runs 77)"},
        {"seconds", "4", "seconds measured a session"},
        {"latencies", "5,40", "one-way latencies, ms, a pair of sessions each"},
    },
    "--seconds=2 --latencies=5",
    run,
};
GE_LAB(kSpec);

}  // namespace
