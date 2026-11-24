// LAB `se-q2-teleport`: HOW SOON DOES THE WINDOW JOINED TO A SERVER OF QUAKE II SHOW A TELEPORT?
// Our Quake II server on engine/net's simulated network, the window's player walking forward onto a
// teleporter pad of a real map (`--game`, `--map`, `--tele`: the map's misc_teleporter by its order
// in the entity text) or of a hall built here, at each one-way latency in `--latencies`, twice: the
// teleport foreseen (`cl_predict_teleport 1`: the window's own prediction takes the pad, puts him
// at the far end, holds him as the move does and turns his view, classic/teleporters.hpp) and not
// (the window waits for the server's frame). The mouse turns a little as the server teleports him.
//
// What it answers, a row a latency and a side: the window frames from walking onto the pad (the
// server's teleport less the one-way latency) to the drawn eye at the far end and to the drawn view
// facing the far end's way; the largest correction drawn; the mouse's turn the view kept. Charted:
// the drawn view's yaw off the far end's, frame by frame from walking in.
//
// ge:summary Lab: Quake II's joined window through a teleporter: the far end and the turn, foreseen
// ge:keywords lab se-q2-teleport teleport teleporter cl_predict_teleport misc_teleporter q2
// ge:keywords predict-teleport frames-to-far-end delta-angles
// ge:skill net labs
// ge:test tests/classic_teleporters_test.cpp
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classic/teleporters.hpp"
#include "classicenhance/profile.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace br = ge::brush;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace sl = shooters_edge::labs;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace gc = ge::classic;
using ge::core::num::to_fixed;

constexpr float kMouseTurn = 20;  // degrees the mouse turns as the server teleports him
constexpr double kAfterS = 0.6;   // s sampled after the server's teleport
constexpr float kFacingDeg = 5;   // the drawn view within this of the far end's way faces it

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A hall of 2048 units with a pad in the west half sending the player to the north-east corner,
// turned to face south.
std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-700 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "misc_teleporter"}, {"origin", "-400 -400 24"}, {"target", "far"}}));
  w.entities.push_back(entity({{"classname", "misc_teleporter_dest"},
                               {"targetname", "far"},
                               {"origin", "600 600 24"},
                               {"angle", "270"}}));
  std::vector<uint8_t> bytes;
  if (!br::write_brush_world(w, &bytes, &error)) return nullptr;
  auto map = std::make_shared<br::Map>();
  if (!br::read_bsp(bytes, map.get(), &error)) return nullptr;
  return map;
}

// The map asked for (`--game` and `--map`: the player's own packs), or the hall.
std::shared_ptr<const br::Map> lab_map(gl::Lab& lab, std::string& name, std::string& error) {
  if (lab.arg("game").empty()) {
    name = "hall";
    return hall(error);
  }
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content))
    return nullptr;
  name = lab.arg("map");
  std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, name, error);
  auto map = std::make_shared<br::Map>();
  if (!m || !br::read_bsp(m->file, map.get(), &error)) return nullptr;
  return map;
}

struct Session {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::Q2NetServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0, wall = 0;  // the server's host ticks, the window's clock
};

bool open_session(Session& s, const std::shared_ptr<const br::Map>& map, const std::string& name,
                  double latency_ms, std::string& error) {
  s.rules = sv::q2_runner(sv::make_q2_server({}));
  s.rules.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = name;
  spec.brushes = map;
  if (!s.rules.start(spec, error)) return false;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(3, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortQ2));
  if (!cn::q2_net_start(s.server, s.rules, s.st, {}, error)) return false;
  gn::open_sim_transport(s.ct, s.net, gn::sim_address(1, 27901));
  s.remote.q2_game = [] { return true; };
  s.remote.read_file = [](const std::string&, const std::string&) {
    return std::vector<uint8_t>{1};  // it has every file
  };
  cn::ClientConfig wc;
  wc.name = "window";
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortQ2), wc);
  return true;
}

void window_frame(Session& s, int64_t window_us) {
  s.wall += window_us;
  while (s.now + cn::kTickUs <= s.wall) {
    s.now += cn::kTickUs;
    gn::sim_advance(s.net, s.now);
    cn::q2_net_frame(s.server, s.now);
  }
  cn::remote_step(s.remote, static_cast<double>(window_us) * 1e-6, s.said);
}

float flat(sh::Vec3f a, sh::Vec3f b) {
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

float wrap180(float d) {
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

sh::Vec3f unpack(const int32_t v[3]) {
  return {static_cast<float>(v[0]) * 0.125f, static_cast<float>(v[1]) * 0.125f,
          static_cast<float>(v[2]) * 0.125f};
}

struct Run {
  bool ok = false, teleported = false;
  int frames_to_far_end = -1;  // window frames from walking in to the drawn eye at the far end
  int frames_to_facing = -1;   // ... to the drawn view facing the far end's way
  double worst_correction = 0;
  double turn_kept = 0;
  std::vector<double> t, off;  // ms from walking in; the drawn yaw off the far end's
};

Run walk(const std::shared_ptr<const br::Map>& map, const std::string& name,
         const sh::Teleporter& tl, sh::Vec3f start, float yaw, double latency_ms, bool foresee,
         int fps) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, map, name, latency_ms, error)) return out;
  const int64_t window_us = 1000000 / fps;
  for (int i = 0; i < 6 * fps && (!s.remote.q2 || s.remote.q2->stage != cn::ClientStage::active);
       i++)
    window_frame(s, window_us);
  if (!s.remote.q2 || s.remote.q2->stage != cn::ClientStage::active) return out;
  cn::remote_world(s.remote, nullptr, map.get());
  cn::remote_teleporters(s.remote, map.get());
  s.remote.predict_teleports = foresee;
  sv::Q2Server* q = sv::q2_of(s.rules);
  const cl::DemoState& d = s.remote.demo.state;
  const int slot = d.player;
  if (q == nullptr || slot < 0) return out;
  // Set on his spot facing the pad, a second to settle; the view's turn the server's alone.
  sh::Vec3f view_turn;
  for (int i = 0; i < fps; i++) {
    gc::Q2State& m = q->players[static_cast<size_t>(slot)].move;
    for (int k = 0; k < 3; k++) m.velocity[k] = 0;
    const float at[3] = {start.x, start.y, start.z};
    for (int k = 0; k < 3; k++)
      m.origin[k] = static_cast<int32_t>(ge::core::num::to_int32(double(at[k]) * 8));
    window_frame(s, window_us);
    view_turn = d.angles - s.remote.view;
  }
  s.remote.view = sh::Vec3f{0, yaw, 0} - view_turn;
  s.remote.hands.forward = 400;
  struct Frame {
    int64_t wall;
    sh::Vec3f eye;
    float yaw;
    double correction;
  };
  std::vector<Frame> frames;
  int64_t teleported_us = -1;
  sh::Vec3f last = unpack(q->players[static_cast<size_t>(slot)].move.origin);
  for (int i = 0; i < 4 * fps; i++) {
    window_frame(s, window_us);
    const sh::Vec3f now = unpack(q->players[static_cast<size_t>(slot)].move.origin);
    if (teleported_us < 0 && flat(now, last) > 96) {
      teleported_us = s.now;
      out.teleported = true;
      s.remote.view.y += kMouseTurn;
    }
    last = now;
    frames.push_back({s.wall, d.eye, d.angles.y,
                      static_cast<double>(sh::length(s.remote.q2_ahead.smooth.correction))});
    if (teleported_us >= 0 && static_cast<double>(s.wall - teleported_us) > kAfterS * 1e6) break;
  }
  if (!out.teleported) return out;
  const int64_t walked_in = teleported_us - static_cast<int64_t>(latency_ms * 1000);
  const sh::Vec3f pad = (tl.lo + tl.hi) * 0.5f;
  for (const Frame& f : frames) {
    if (f.wall < walked_in) continue;
    const bool far_end = flat(f.eye, tl.dest) < flat(f.eye, pad);
    const double off = std::fabs(wrap180(f.yaw - tl.angles.y));
    if (out.frames_to_far_end < 0 && far_end)
      out.frames_to_far_end = static_cast<int>(out.t.size());
    if (out.frames_to_facing < 0 && off < kFacingDeg + kMouseTurn && far_end)
      out.frames_to_facing = static_cast<int>(out.t.size());
    out.t.push_back(static_cast<double>(f.wall - walked_in) / 1000.0);
    out.off.push_back(off);
    out.worst_correction = std::fmax(out.worst_correction, f.correction);
  }
  if (out.frames_to_far_end < 0) out.frames_to_far_end = static_cast<int>(out.t.size());
  if (out.frames_to_facing < 0) out.frames_to_facing = static_cast<int>(out.t.size());
  out.turn_kept = wrap180(frames.back().yaw - tl.angles.y);
  out.ok = true;
  return out;
}

void run(gl::Lab& lab) {
  const int fps = static_cast<int>(lab.arg_num("fps"));
  const int which = static_cast<int>(lab.arg_num("tele"));
  std::vector<double> latencies;
  const std::string list = lab.arg("latencies");
  for (size_t at = 0; at < list.size();) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    latencies.push_back(std::atof(list.substr(at, comma - at).c_str()));
    at = comma + 1;
  }
  std::string name, error;
  const std::shared_ptr<const br::Map> map = lab_map(lab, name, error);
  if (map == nullptr) {
    lab.fail("se-q2-teleport: the map did not load: " + error,
             "--game is the folder holding Quake II's base folder, --map a map in it. Skill: "
             "brush.");
    return;
  }
  const sh::Teleporters teles = gc::q2_teleporters(*map);
  if (fps < 30 || latencies.empty() || which < 0 || which >= static_cast<int>(teles.size())) {
    lab.fail(
        "se-q2-teleport: --fps must be 30 or more, --latencies name one or more and --tele "
        "one of the map's " +
            std::to_string(teles.size()) + " teleporters",
        "next: make lab LAB=se-q2-teleport ARGS=--help");
    return;
  }
  const sh::Teleporter& tl = teles[static_cast<size_t>(which)];
  // The way in: of the four sides, the first a walk from 150 units out takes him through.
  const sh::Vec3f mid = (tl.lo + tl.hi) * 0.5f;
  sh::Vec3f start;
  float yaw = 0;
  bool found = false;
  for (int side = 0; side < 4 && !found; side++) {
    const float cs = side == 0 ? 1.f : side == 2 ? -1.f : 0.f;
    const float sn = side == 1 ? 1.f : side == 3 ? -1.f : 0.f;
    start = {mid.x - cs * 150, mid.y - sn * 150, tl.lo.z + 9};
    yaw = static_cast<float>(side) * 90.0f;
    const Run probe = walk(map, name, tl, start, yaw, 0, false, fps);
    found = probe.ok && probe.teleported;
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"game", lab.arg("game")}, {"map", name}, {"tele", lab.arg("tele")}, {"latencies", list}};
  if (!found) {
    lab.fail("se-q2-teleport: no walk from any side took the player through teleporter " +
                 lab.arg("tele") + " of " + name,
             "Name another (--tele) or read the walk in games/shooters_edge/labs/"
             "se_q2_teleport_lab.cpp. Skill: net.");
    return;
  }
  gl::LabTable t{"walking onto pad " + lab.arg("tele") + " of " + name + ", the mouse turning " +
                     to_fixed(kMouseTurn, 0) + " degrees as he goes",
                 {"one way ms", "the teleport", "frames to the far end", "frames to its facing",
                  "worst correction (units)", "mouse turn kept (deg)"},
                 {}};
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"QUAKE II THROUGH A TELEPORTER: THE VIEW AGAINST THE FAR END'S FACING",
             name + ", the window at " + std::to_string(fps) + " fps",
             "since he walked onto the pad (ms)", "degrees off the far end's facing"};
  for (double ms : latencies) {
    for (const bool foresee : {true, false}) {
      const Run r = walk(map, name, tl, start, yaw, ms, foresee, fps);
      if (!r.ok || !r.teleported) {
        lab.fail("se-q2-teleport: at " + to_fixed(ms, 0) + " ms the window never went through",
                 "next: build/debug/tests/classic_teleporters_test. Skill: net.");
        return;
      }
      const std::string tag = foresee ? "foreseen" : "waited";
      const std::string key = "ms-" + to_fixed(ms, 0) + "." + tag;
      t.rows.push_back({to_fixed(ms, 0), tag, std::to_string(r.frames_to_far_end),
                        std::to_string(r.frames_to_facing), to_fixed(r.worst_correction, 1),
                        to_fixed(r.turn_kept, 1)});
      lab.metric(key + ".frames-to-far-end", r.frames_to_far_end, "frames");
      lab.metric(key + ".frames-to-facing", r.frames_to_facing, "frames");
      lab.metric(key + ".worst-correction", r.worst_correction, "units");
      lab.metric(key + ".turn-kept", r.turn_kept, "deg");
      c.traces.push_back({to_fixed(ms, 0) + " ms " + tag, r.t, r.off});
      if (foresee && (r.frames_to_far_end > 2 || r.frames_to_facing > 2 ||
                      std::fabs(r.turn_kept - kMouseTurn) > 1))
        lab.fail("se-q2-teleport: at " + to_fixed(ms, 0) +
                     " ms each way with the teleport foreseen the far end came " +
                     std::to_string(r.frames_to_far_end) + " frames and its facing " +
                     std::to_string(r.frames_to_facing) +
                     " frames after he walked on, the view "
                     "keeping " +
                     to_fixed(r.turn_kept, 1) + " of the mouse's " + to_fixed(kMouseTurn, 0) +
                     " degrees",
                 "next: build/debug/tests/classic_teleporters_test. Skill: net.");
    }
  }
  lab.table(t);
  sl::shoot(lab, "facing", gl::line_chart(c),
            "Each window frame from the moment he walked onto the pad: how far the drawn view is "
            "from the far end's facing (the mouse's own turn of " +
                to_fixed(kMouseTurn, 0) +
                " degrees on top once through). Foreseen, the view turns the frame he steps on; "
                "waited, it holds the old way for the round trip.",
            "every 'foreseen' line drops at once; the 'waited' ones a round trip later", in);
}

const gl::LabSpec kSpec{
    "se-q2-teleport",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "classic/teleporters.hpp's Quake II teleporters taken in the joined window's prediction "
    "(q2_predict.hpp), over the simulated network",
    "net",
    "How soon does the window joined to a server of Quake II draw a teleport's far end and turn "
    "its view, foreseen and not, at each latency?",
    {
        {"fps", "144", "the window's frames a second"},
        {"latencies", "0,25,50", "one-way latencies, ms, two walks each (foreseen, waited)"},
        {"game", "", "the folder holding Quake II's base folder (none: the hall built here)"},
        {"map", "q2dm1", "the map walked, with --game"},
        {"tele", "0", "the teleporter: its order among the map's misc_teleporter entities"},
    },
    "--latencies=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
