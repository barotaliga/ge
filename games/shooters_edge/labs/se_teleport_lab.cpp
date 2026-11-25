// LAB `se-teleport`: DOES A PLAYER KEEP CONTROL THROUGH A TELEPORTER? The window joined to the
// QuakeWorld server (the arena's rules in C++) on engine/net's simulated network walks forward
// into a teleporter of a real map (`--basedir`, `--map`, `--tele`: the map's teleporter by its
// order in the entity text; without --basedir the arena with its doorway made a teleporter) and
// keeps walking forward on the far side, while the mouse turns the view a little the moment the
// server teleports him. Each one-way latency in `--latencies` runs four ways:
//
// - PREDICTED: the teleport foreseen (`cl_predict_teleport 1`, shooter/teleport_ahead.hpp): the
//   window's prediction takes the level's teleporter the command he walks in, the far end drawn
//   and the view turned at once; the high-lag teleport not asked for;
// - FIXED: the high-lag teleport on both sides (`cl_pext_lagteleport 1`: the server's turn sent
//   with its kind, the moves in flight turned on the server and in the window's prediction alike,
//   classic/protocol/lag_turn.hpp);
// - WINDOW BEHIND: the server turns the moves in flight but the window replays them as made, as
//   the window did before (it asked for the kind and let it be): every teleport a correction;
// - NO EXTENSION: neither side turns them (a server or client without it): the moves go the old
//   way for a round trip.
//
// Each window frame from the moment he walked in (the server's teleport less the one-way latency)
// is sampled: where the window draws him against
// where he faces (the angle between his drawn motion and his view), the prediction's correction,
// and whether the view took the server's turn.
//
// What it answers, a row a latency and a side: the frames until the drawn eye is at the far end;
// the frames after it that move him more than 25 degrees off where he faces (the lost control);
// the largest correction drawn; and whether the mouse's turn during the round trip was kept.
// Charted: the angle off his facing frame by frame through the teleport, at each latency.
//
// ge:summary Lab: walking through a teleporter joined to a server: control kept frame by frame
// ge:keywords lab se-teleport teleport teleporter control lag-teleport cl_pext_lagteleport
// ge:keywords cl_predict_teleport predict-teleport
// ge:keywords setangle in-flight-commands prediction correction turn latency frames-lost
// ge:skill net labs
// ge:test tests/classic_lag_turn_test.cpp
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
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
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;

constexpr double kPi = 3.14159265358979323846;
constexpr float kMouseTurn = 20;     // degrees the mouse turns as the server teleports him
constexpr float kOffFacing = 25;     // degrees off his facing that count as lost control
constexpr double kAfterS = 0.6;      // s sampled after the server's teleport
constexpr float kSettledSpeed = 60;  // units/s: slower is standing, not steering

// The arena with its doorway (the door's brush) a teleporter to the far corner of the east half,
// turned to face south: the fallback when no game folder is named.
const char* const kArenaEntities = R"({
"classname" "worldspawn"
"message" "the teleporter lab"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 0 24"
}
{
"classname" "trigger_teleport"
"model" "*1"
"target" "far"
}
{
"classname" "info_teleport_destination"
"targetname" "far"
"origin" "400 380 0"
"angle" "270"
}
)";

struct Tele {
  sh::Vec3f lo, hi;  // the trigger's box
  sh::Vec3f dest;    // the destination's origin
  float dest_yaw = 0;
};

struct Session {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::QwServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0;
};

struct Map {
  std::unique_ptr<cl::LoadedMap> loaded;  // a game folder's (null: the arena)
  sv::MapSpec spec;
  std::vector<sh::BrushModel> arena;
  ge::brush::Map arena_level;             // the arena's entities and model boxes, as a BSP view
  const ge::brush::Map* level = nullptr;  // the window's view of the level (its teleporters)
  std::vector<Tele> teles;
};

enum class Mode : uint8_t { predicted, fixed, window_behind, no_extension };
const char* mode_name(Mode m) {
  return m == Mode::predicted       ? "predicted"
         : m == Mode::fixed         ? "fixed"
         : m == Mode::window_behind ? "window behind"
                                    : "no extension";
}

bool open_session(Session& s, Map& m, double latency_ms, Mode mode, std::string& error) {
  if (!sv::baseline_mod(s.progs, error)) return false;
  sv::QcServerOptions so;
  so.loop.max_edicts = 2048;
  s.qc = sv::make_qc_server(s.progs, so, error);
  if (!s.qc || !sv::load_map(*s.qc, m.spec, error)) return false;
  s.rules = sv::qc_runner_over(*s.qc);
  if (m.loaded) sv::use_map_sets(*s.rules, m.loaded->bsp);
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(1, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortServer));
  gn::open_sim_transport(s.ct, s.net, gn::sim_address(1, cn::kPortClient));
  if (!cn::qw_start(s.server, s.rules, s.st, cn::ServerConfig{}, error)) return false;
  cn::ClientConfig wc;
  wc.name = "window";
  wc.lag_turn = mode == Mode::fixed;
  if (mode == Mode::no_extension || mode == Mode::predicted)
    wc.ask[cn::ExtFamily::mvd] &= ~cn::kMvdTeleport;
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortServer), wc);
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

float flat_distance(sh::Vec3f a, sh::Vec3f b) {
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

float wrap180(float d) {
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

struct Run {
  bool ok = false;
  bool teleported = false;
  bool agreed = false;          // the high-lag teleport agreed
  int frames_to_far_end = -1;   // window frames from the server's teleport to the eye there
  int lost = 0;                 // frames after it moving him more than kOffFacing off his facing
  double worst_correction = 0;  // units
  double turn_kept = 0;         // degrees of the mouse's turn the view still holds after
  float yaw_after = 0;          // the view's yaw at the end
  std::vector<double> t, off;   // ms from the server's teleport; degrees off his facing
};

// One walk into teleporter `tl` from `start` facing `yaw` at `latency_ms` each way.
Run walk(Map& m, const Tele& tl, sh::Vec3f start, float yaw, double latency_ms, Mode mode,
         int fps) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, m, latency_ms, mode, error)) return out;
  const int64_t window_us = 1000000 / fps;
  int64_t wall = 0;
  for (int i = 0; i < 8 * fps && s.remote.end.stage != cn::ClientStage::active; i++)
    window_frame(s, wall += window_us, window_us);
  if (s.remote.end.stage != cn::ClientStage::active) return out;
  cn::remote_world(s.remote, &m.spec.models);
  s.remote.predict_teleports = mode == Mode::predicted;
  cn::remote_teleporters(s.remote, mode == Mode::predicted ? m.level : nullptr);
  out.agreed = s.remote.end.ext.has(cn::ExtFamily::mvd, cn::kMvdTeleport);
  const int slot = s.remote.end.playernum;
  sh::Edict& me = s.rules->sv.edicts[static_cast<size_t>(slot) + 1];
  // Set on his spot facing the teleporter, a second to settle.
  s.remote.view = {0, yaw, 0};
  for (int i = 0; i < fps; i++) {
    me.origin = start;
    me.velocity = {};
    window_frame(s, wall += window_us, window_us);
  }
  s.remote.hands.forward = 400;
  const cl::DemoState& d = s.remote.demo.state;
  // Every window frame kept, then read from the moment he walked in: the server's teleport less
  // the one-way latency (the command that took him there was sent then).
  struct Frame {
    int64_t wall;
    sh::Vec3f eye;
    float yaw;
    double correction;
  };
  std::vector<Frame> frames;
  sh::Vec3f last_server = me.origin;
  int64_t teleported_us = -1;
  for (int i = 0; i < 4 * fps; i++) {
    window_frame(s, wall += window_us, window_us);
    if (teleported_us < 0 && flat_distance(me.origin, last_server) > 96) {
      teleported_us = s.now;
      out.teleported = true;
      s.remote.view.y += kMouseTurn;  // the mouse turns as he goes through: a player always does
    }
    last_server = me.origin;
    frames.push_back({wall, d.eye, s.remote.view.y,
                      static_cast<double>(sh::length(s.remote.smooth.correction))});
    if (teleported_us >= 0 && static_cast<double>(wall - teleported_us) > kAfterS * 1e6) break;
  }
  if (!out.teleported) return out;
  const int64_t walked_in = teleported_us - static_cast<int64_t>(latency_ms * 1000);
  for (size_t i = 1; i < frames.size(); i++) {
    const Frame& f = frames[i];
    if (f.wall < walked_in) continue;
    const double ms = static_cast<double>(f.wall - walked_in) / 1000.0;
    const float dx = f.eye.x - frames[i - 1].eye.x, dy = f.eye.y - frames[i - 1].eye.y;
    const double speed = std::sqrt(dx * dx + dy * dy) * static_cast<double>(fps);
    const bool far_end =
        flat_distance(f.eye, tl.dest) < flat_distance(f.eye, (tl.lo + tl.hi) * 0.5f);
    if (out.frames_to_far_end < 0 && far_end)
      out.frames_to_far_end = static_cast<int>(out.t.size());
    double off = 0;
    if (far_end && speed > kSettledSpeed && speed < 3000) {
      const double moving = ge::core::libm::atan2(dy, dx) * 180 / kPi;
      off = std::fabs(wrap180(static_cast<float>(moving - f.yaw)));
      if (off > kOffFacing) out.lost++;
    } else if (!far_end) {
      off = 180;  // still drawn at the near end: no control at all
    }
    out.t.push_back(ms);
    out.off.push_back(off);
    out.worst_correction = std::fmax(out.worst_correction, f.correction);
  }
  if (out.frames_to_far_end < 0) out.frames_to_far_end = static_cast<int>(out.t.size());
  out.yaw_after = s.remote.view.y;
  // The mouse's turn kept: how far the view is from the destination's facing, as turned by the
  // mouse after the teleport (kMouseTurn when all of it is kept, 0 when the server's turn threw
  // it away).
  out.turn_kept = wrap180(out.yaw_after - tl.dest_yaw);
  out.ok = true;
  return out;
}

bool read_vec(const std::string& text, sh::Vec3f& v) {
  return std::sscanf(text.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3;
}

std::string get(const std::vector<std::pair<std::string, std::string>>& e, const std::string& k) {
  for (const auto& [a, b] : e)
    if (a == k) return b;
  return {};
}

using Ents = std::vector<std::vector<std::pair<std::string, std::string>>>;

// The entity text's entities, each its keys and values in order.
Ents parse_entities(const std::string& text) {
  Ents out;
  size_t at = 0;
  while ((at = text.find('{', at)) != std::string::npos) {
    const size_t end = text.find('}', at);
    if (end == std::string::npos) break;
    std::vector<std::pair<std::string, std::string>> e;
    size_t p = at;
    for (;;) {
      const size_t k0 = text.find('"', p);
      if (k0 == std::string::npos || k0 > end) break;
      const size_t k1 = text.find('"', k0 + 1);
      const size_t v0 = text.find('"', k1 + 1);
      const size_t v1 = text.find('"', v0 + 1);
      if (v1 == std::string::npos || v1 > end) break;
      e.emplace_back(text.substr(k0 + 1, k1 - k0 - 1), text.substr(v0 + 1, v1 - v0 - 1));
      p = v1 + 1;
    }
    out.push_back(std::move(e));
    at = end + 1;
  }
  return out;
}

// The teleporters of the map's entity text: their boxes (their models') and destinations, 27
// units above the marker as the rules put a player.
void teles_of(Map& m) {
  const Ents ents = parse_entities(m.spec.entities);
  for (const auto& e : ents) {
    if (get(e, "classname") != "trigger_teleport") continue;
    const std::string model = get(e, "model");
    if (model.size() < 2 || model[0] != '*') continue;
    const size_t n = static_cast<size_t>(std::atoi(model.c_str() + 1));
    if (n >= m.spec.models.size()) continue;
    Tele t;
    t.lo = m.spec.models[n].mins;
    t.hi = m.spec.models[n].maxs;
    bool found = false;
    for (const auto& d : ents) {
      if (get(d, "targetname") != get(e, "target") || get(e, "target").empty()) continue;
      found = read_vec(get(d, "origin"), t.dest);
      t.dest.z += 27;
      t.dest_yaw = static_cast<float>(std::atof(get(d, "angle").c_str()));
      sh::Vec3f mangle;
      if (read_vec(get(d, "mangle"), mangle)) t.dest_yaw = mangle.y;
      break;
    }
    if (found) m.teles.push_back(t);
  }
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
  Map m;
  std::string name = "arena", error;
  const std::string basedir = lab.arg("basedir");
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail("se-teleport: " + error,
               "Pass --basedir=<the folder that holds the game folder>. Skill: net.");
      return;
    }
    name = lab.arg("map");
    m.loaded = cl::load_named_map(files, name, error);
    if (!m.loaded) {
      lab.fail("se-teleport: " + error, "Name a map the game folder holds (--map). Skill: net.");
      return;
    }
    m.spec = m.loaded->spec;
  } else {
    m.arena = sv::arena_models(true);
    m.spec.name = "arena";
    m.spec.models = m.arena;
    m.spec.entities = kArenaEntities;
    std::string why;
    ge::brush::parse_entities(kArenaEntities, &m.arena_level.entities, &why);
    for (const sh::BrushModel& b : m.arena) {
      ge::brush::Model bm;
      for (int k = 0; k < 3; k++) {
        bm.mins[k] = b.mins[k];
        bm.maxs[k] = b.maxs[k];
      }
      m.arena_level.models.push_back(bm);
    }
  }
  m.level = m.loaded ? &m.loaded->bsp.map : &m.arena_level;
  teles_of(m);
  if (fps < 30 || latencies.empty() || which < 0 || which >= static_cast<int>(m.teles.size())) {
    lab.fail(
        "se-teleport: --fps must be 30 or more, --latencies name one or more and --tele "
        "one of the map's " +
            std::to_string(m.teles.size()) + " teleporters",
        "next: make lab LAB=se-teleport ARGS=--help");
    return;
  }
  const Tele& tl = m.teles[static_cast<size_t>(which)];
  // The way in: of the four sides, the first a walk from 200 units out takes him through.
  const sh::Vec3f mid = (tl.lo + tl.hi) * 0.5f;
  sh::Vec3f start;
  float yaw = 0;
  bool found = false;
  for (int side = 0; side < 4 && !found; side++) {
    const float a = static_cast<float>(side) * 90.0f;
    const float half = side % 2 == 0 ? (tl.hi.x - tl.lo.x) * 0.5f : (tl.hi.y - tl.lo.y) * 0.5f;
    const float cs = side == 0 ? 1.f : side == 2 ? -1.f : 0.f;
    const float sn = side == 1 ? 1.f : side == 3 ? -1.f : 0.f;
    const float back = half + 200;
    start = {mid.x - cs * back, mid.y - sn * back, tl.lo.z + 25};
    yaw = a;
    const Run probe = walk(m, tl, start, yaw, 0, Mode::fixed, fps);
    found = probe.ok && probe.teleported;
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"basedir", basedir}, {"map", name}, {"tele", lab.arg("tele")}, {"latencies", list}};
  if (!found) {
    lab.fail("se-teleport: no walk from any side took the player through teleporter " +
                 lab.arg("tele") + " of " + name,
             "Name another (--tele) or read the walk in games/shooters_edge/labs/"
             "se_teleport_lab.cpp. Skill: net.");
    return;
  }
  gl::LabTable t{"walking forward through teleporter " + lab.arg("tele") + " of " + name +
                     ", the mouse turning " + to_fixed(kMouseTurn, 0) + " degrees as he goes",
                 {"one way ms", "the moves in flight", "frames to the far end", "frames off facing",
                  "worst correction (units)", "mouse turn kept (deg)"},
                 {}};
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THROUGH A TELEPORTER: WHERE YOU MOVE AGAINST WHERE YOU FACE",
             name + ", the window at " + std::to_string(fps) +
                 " fps; 180 while still drawn at the near end",
             "since he walked in (ms)", "degrees off his facing"};
  for (double ms : latencies) {
    for (const Mode mode :
         {Mode::predicted, Mode::fixed, Mode::window_behind, Mode::no_extension}) {
      const bool fixed = mode == Mode::fixed;
      const Run r = walk(m, tl, start, yaw, ms, mode, fps);
      if (!r.ok || !r.teleported) {
        lab.fail("se-teleport: at " + to_fixed(ms, 0) + " ms the window never went through",
                 "next: build/debug/tests/classic_lag_turn_test. Skill: net.");
        return;
      }
      std::string tag = mode_name(mode);
      for (char& ch : tag) ch = ch == ' ' ? '-' : ch;
      const std::string key = "ms-" + to_fixed(ms, 0) + "." + tag;
      t.rows.push_back({to_fixed(ms, 0), mode_name(mode), std::to_string(r.frames_to_far_end),
                        std::to_string(r.lost), to_fixed(r.worst_correction, 1),
                        to_fixed(r.turn_kept, 1)});
      lab.metric(key + ".frames-to-far-end", r.frames_to_far_end, "frames");
      lab.metric(key + ".frames-off-facing", r.lost, "frames");
      lab.metric(key + ".worst-correction", r.worst_correction, "units");
      lab.metric(key + ".turn-kept", r.turn_kept, "deg");
      c.traces.push_back({to_fixed(ms, 0) + " ms " + mode_name(mode), r.t, r.off});
      if (mode == Mode::predicted &&
          (r.frames_to_far_end > 2 || r.lost > 1 || std::fabs(r.turn_kept - kMouseTurn) > 1))
        lab.fail("se-teleport: at " + to_fixed(ms, 0) +
                     " ms each way with the teleport foreseen the far end came " +
                     std::to_string(r.frames_to_far_end) + " frames after he walked in, " +
                     std::to_string(r.lost) + " frames off his facing, and the view kept " +
                     to_fixed(r.turn_kept, 1) + " of the mouse's " + to_fixed(kMouseTurn, 0) +
                     " degrees",
                 "next: build/debug/tests/shooter_teleport_ahead_test. Skill: net.");
      if (fixed && (!r.agreed || r.lost > 1 || std::fabs(r.turn_kept - kMouseTurn) > 1))
        lab.fail("se-teleport: at " + to_fixed(ms, 0) +
                     " ms each way with the high-lag "
                     "teleport the player moved off his facing for " +
                     std::to_string(r.lost) + " frames and kept " + to_fixed(r.turn_kept, 1) +
                     " of the mouse's " + to_fixed(kMouseTurn, 0) + " degrees",
                 "next: build/debug/tests/classic_lag_turn_test. Skill: net.");
    }
  }
  lab.table(t);
  sl::shoot(lab, "off-facing", gl::line_chart(c),
            "Each window frame from the moment he walked in: the angle between where the window "
            "draws him moving and where he faces. 180 while he is still drawn at the near end; on "
            "the far side, near 0 is full control. Predicted, he is at the far end facing its way "
            "from the frame he walks in; fixed, a round trip later and then where he faces; with "
            "the window behind, the correction pulls him off it; with no extension, the moves he "
            "made before the turn reached him walk him the old way for a round trip.",
            "every 'predicted' line near 0 from the start; 'fixed' drops to near 0 a round trip "
            "later; the others sit high for a while after it",
            in);
}

const gl::LabSpec kSpec{
    "se-teleport",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "classic/protocol/lag_turn.hpp and the QuakeWorld server's turn (setangle) through a "
    "teleporter, joined over the simulated network",
    "net",
    "Does a player walking through a teleporter on a server keep control of where he goes and "
    "faces, frame by frame, at each latency?",
    {
        {"fps", "144", "the window's frames a second (the server runs 77)"},
        {"latencies", "0,25,50",
         "one-way latencies, ms, four walks each (predicted, fixed, the "
         "window behind, no extension)"},
        {"basedir", "",
         "the folder that holds the game folder (\"\": the arena, its doorway a "
         "teleporter)"},
        {"game", "", "a mod folder over the game folder"},
        {"map", "dm6", "the map walked, with --basedir"},
        {"tele", "0", "the teleporter: its order among the map's trigger_teleport entities"},
    },
    "--latencies=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
