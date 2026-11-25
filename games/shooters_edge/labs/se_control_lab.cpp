// LAB `se-control`: WHERE DOES THE JOINED WINDOW LAG OR YANK THE PLAYER? The window joined to the
// QuakeWorld server (the arena's rules in C++) or, for a Quake II install (`--game` a folder that
// holds `baseq2`), to our Quake II server, on engine/net's simulated network, meets one of a real
// map's moments (`--basedir`, `--map`, `--event`, `--index`: the map's entity of that kind by its
// order in the entity text) and is sampled every window frame:
//
// - PLAT: dropped onto a `func_plat`, he rides it up and back down;
// - TRAIN: dropped onto a `func_train`, he rides it along its path;
// - DOOR: he runs at a `func_door` from 160 units out and through it as it opens;
// - PUSH: he walks into a `trigger_push` (a wind tunnel or a jump pad) and is thrown by it;
// - WATER: dropped into a body of water (`--index`: the body by size, largest first), he swims
//   forward and up until he climbs out or the time runs out.
//
// Each one-way latency in `--latencies` runs twice: as the window is built now (`foreseen`: the
// level's push triggers taken in the prediction, `cl_predict_push 1`, and its movers run on through
// it, `cl_predict_movers 1`) and without (`waited`: both 0, the server's word waited for).
//
// What it answers, a row a latency and a way: the largest miss (how far a frame from the server
// moved the window's prediction, before any smoothing eases it), the frames that moved it more
// than half a unit (a yank: the server disagreed with what the window drew), and the total
// pulled. Charted: the miss frame by frame.
//
// ge:summary Lab: plats, trains, doors, push triggers and water joined to a server: yanks counted
// ge:keywords lab se-control control yank correction prediction plat train door trigger_push
// ge:keywords jump-pad wind-tunnel water movers cl_predict_push cl_predict_movers latency q2
// ge:skill net labs
// ge:test tests/shooter_teleport_ahead_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/survey.hpp"
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
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace sl = shooters_edge::labs;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace br = ge::brush;
using ge::core::num::to_fixed;

constexpr float kYank = 0.5f;  // units: the server's word moving the prediction more is a yank

enum class Event : uint8_t { plat, train, door, push, water };

// A QuakeWorld map (`loaded`, `spec`) or a Quake II one (`q2`).
struct Map {
  std::unique_ptr<cl::LoadedMap> loaded;
  sv::MapSpec spec;
  std::shared_ptr<const br::Map> q2;
  std::string name;
  const br::Map& bsp() const { return q2 ? *q2 : loaded->bsp.map; }
};

struct Session {
  bool q2 = false;
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::QwServer server;
  cn::Q2NetServer q2server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0;
};

// Where he starts and what his hands hold.
struct Drive {
  sh::Vec3f start;
  sh::Vec3f aim;     // the entity's middle
  sh::Vec3f lo, hi;  // its box
  int model = -1;    // a mover ridden: its brush model, dropped onto where the server has it
  float yaw = 0;
  int16_t forward = 0, up = 0;
  bool jump = false;
  double seconds = 4;
};

bool open_q2(Session& s, const Map& m, double latency_ms, std::string& error) {
  s.q2 = true;
  s.rules = sv::q2_runner(sv::make_q2_server({}));
  s.rules.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = m.name;
  spec.brushes = m.q2;
  if (!s.rules.start(spec, error)) return false;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(3, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortQ2));
  if (!cn::q2_net_start(s.q2server, s.rules, s.st, {}, error)) return false;
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

bool open_session(Session& s, Map& m, double latency_ms, std::string& error) {
  if (m.q2) return open_q2(s, m, latency_ms, error);
  if (!sv::baseline_mod(s.progs, error)) return false;
  sv::QcServerOptions so;
  so.loop.max_edicts = 2048;
  s.qc = sv::make_qc_server(s.progs, so, error);
  if (!s.qc || !sv::load_map(*s.qc, m.spec, error)) return false;
  s.rules = sv::qc_runner_over(*s.qc);
  sv::use_map_sets(*s.rules, m.loaded->bsp);
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
  wc.ask[cn::ExtFamily::mvd] &= ~cn::kMvdTeleport;
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortServer), wc);
  return true;
}

void window_frame(Session& s, int64_t wall_us, int64_t window_us) {
  while (s.now + cn::kTickUs <= wall_us) {
    s.now += cn::kTickUs;
    gn::sim_advance(s.net, s.now);
    if (s.q2)
      cn::q2_net_frame(s.q2server, s.now);
    else
      cn::qw_frame(s.server, s.now);
  }
  cn::remote_step(s.remote, static_cast<double>(window_us) * 1e-6, s.said);
}

bool joined(const Session& s) {
  return s.q2 ? s.remote.q2 && s.remote.q2->stage == cn::ClientStage::active
              : s.remote.end.stage == cn::ClientStage::active;
}

// The server's own player: where he is, how deep in water, and a hand to set him somewhere still.
struct Body {
  sh::Edict* qw = nullptr;
  sv::Q2Player* q2 = nullptr;
  sh::Vec3f origin() const {
    if (qw != nullptr) return qw->origin;
    return {static_cast<float>(q2->move.origin[0]) * 0.125f,
            static_cast<float>(q2->move.origin[1]) * 0.125f,
            static_cast<float>(q2->move.origin[2]) * 0.125f};
  }
  int water() const { return qw != nullptr ? qw->waterlevel : q2->water_level; }
  void place(sh::Vec3f at) const {
    if (qw != nullptr) {
      qw->origin = at;
      qw->velocity = {};
      return;
    }
    const float p[3] = {at.x, at.y, at.z};
    for (int k = 0; k < 3; k++) {
      q2->move.origin[k] = ge::core::num::to_int32(static_cast<double>(p[k]) * 8);
      q2->move.velocity[k] = 0;
    }
  }
};

struct Run {
  bool ok = false;
  double worst = 0;  // the largest miss, units
  int yanks = 0;     // frames a miss was more than kYank
  double pulled = 0;
  int waterlevel = 0;         // the deepest the server had him (water)
  float rose = 0, moved = 0;  // how far the server took him up from his lowest, and across
  float nearest = 1e9f;       // the nearest the server had him to the entity's middle, across
  bool inside = false;        // the server had him in the entity's box, across
  std::vector<double> t, corr;
};

// Where the server has brush model `model` (a plat or a train, which may have moved off its built
// place): the middle of its box, at its top. False when no entity carries it.
bool model_top(Session& s, int model, sh::Vec3f& at) {
  if (s.q2) {
    sv::Q2Server* q = sv::q2_of(s.rules);
    const std::string name = "*" + std::to_string(model);
    for (const sv::Q2Entity& e : q->entities)
      if (e.model == name) {
        at = e.origin + (e.mins + e.maxs) * 0.5f;
        at.z = e.origin.z + e.maxs.z;
        return true;
      }
    return false;
  }
  for (const sh::Edict& e : s.rules->sv.edicts)  // the model list: "", the map, then *1, *2, ...
    if (!e.free && e.modelindex == model + 1) {
      at = {(e.absmin.x + e.absmax.x) * 0.5f, (e.absmin.y + e.absmax.y) * 0.5f, e.absmax.z};
      return true;
    }
  return false;
}

Run drive(Map& m, const Drive& dr, double latency_ms, bool foreseen, int fps) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, m, latency_ms, error)) return out;
  const int64_t window_us = 1000000 / fps;
  int64_t wall = 0;
  for (int i = 0; i < 8 * fps && !joined(s); i++) window_frame(s, wall += window_us, window_us);
  if (!joined(s)) return out;
  Body me;
  if (s.q2) {
    cn::remote_world(s.remote, nullptr, m.q2.get());
    cn::remote_teleporters(s.remote, m.q2.get());
    sv::Q2Server* q = sv::q2_of(s.rules);
    const int slot = s.remote.demo.state.player;
    if (q == nullptr || slot < 0) return out;
    me.q2 = &q->players[static_cast<size_t>(slot)];
  } else {
    cn::remote_world(s.remote, &m.spec.models);
    cn::remote_teleporters(s.remote, &m.loaded->bsp.map);
    me.qw = &s.rules->sv.edicts[static_cast<size_t>(s.remote.end.playernum) + 1];
  }
  s.remote.predict_teleports = true;
  s.remote.predict_pushes = foreseen;
  s.remote.predict_movers = foreseen;
  s.remote.view = {0, dr.yaw, 0};
  sh::Vec3f start = dr.start;
  if (sh::Vec3f top; dr.model > 0 && model_top(s, dr.model, top)) start = top + sh::Vec3f{0, 0, 40};
  for (int i = 0; i < fps / 2; i++) {
    me.place(start);
    window_frame(s, wall += window_us, window_us);
  }
  if (s.q2) {  // the server's view turn undone, so the hands face the drive's way
    const sh::Vec3f turn = s.remote.demo.state.angles - s.remote.view;
    s.remote.view = sh::Vec3f{0, dr.yaw, 0} - turn;
  }
  // Let go and let the window catch up with the server for two round trips and a little before
  // the hands move, so the placing itself is never counted (a drop falls on meanwhile).
  const int still = static_cast<int>((4 * latency_ms + 200) * fps / 1000);
  for (int i = 0; i < still; i++) window_frame(s, wall += window_us, window_us);
  const sh::Vec3f from = me.origin();
  float low = from.z;
  s.remote.hands.forward = dr.forward;
  s.remote.hands.up = dr.up;
  s.remote.hands.buttons = dr.jump ? sh::kButtonJump : 0;
  const auto misses_now = [&] { return s.q2 ? s.remote.q2_ahead.misses : s.remote.misses; };
  int64_t misses = misses_now();
  const int frames = static_cast<int>(dr.seconds * fps);
  for (int i = 0; i < frames; i++) {
    window_frame(s, wall += window_us, window_us);
    // The server's word against the prediction it answers, before any smoothing eases it.
    const float c =
        misses_now() != misses ? (s.q2 ? s.remote.q2_ahead.miss_units : s.remote.miss_units) : 0.0f;
    misses = misses_now();
    if (c > kYank) {
      out.yanks++;
      out.pulled += c;
    }
    out.worst = std::fmax(out.worst, c);
    const sh::Vec3f o = me.origin();
    out.waterlevel = std::max(out.waterlevel, me.water());
    low = std::fmin(low, o.z);
    out.rose = std::fmax(out.rose, o.z - low);
    const float dx = o.x - from.x, dy = o.y - from.y;
    out.moved = std::fmax(out.moved, std::sqrt(dx * dx + dy * dy));
    const float ax = o.x - dr.aim.x, ay = o.y - dr.aim.y;
    out.nearest = std::fmin(out.nearest, std::sqrt(ax * ax + ay * ay));
    out.inside = out.inside || (o.x > dr.lo.x && o.x < dr.hi.x && o.y > dr.lo.y && o.y < dr.hi.y);
    out.t.push_back(static_cast<double>(i) * 1000.0 / fps);
    out.corr.push_back(c);
  }
  out.ok = true;
  return out;
}

bool read_vec(const std::string& text, sh::Vec3f& v) {
  return std::sscanf(text.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3;
}

// The `index`th entity of `kind` in the level: its model's box (with its origin), and its angle.
bool site_of(const Map& m, const std::string& kind, int index, sh::Vec3f& lo, sh::Vec3f& hi,
             float& angle) {
  int seen = 0;
  for (const ge::brush::Entity& e : m.bsp().entities) {
    if (e.get("classname") != kind) continue;
    if (seen++ != index) continue;
    const std::string& model = e.get("model");
    if (model.size() < 2 || model[0] != '*') return false;
    const size_t n = static_cast<size_t>(std::atoi(model.c_str() + 1));
    if (n >= m.bsp().models.size()) return false;
    sh::Vec3f at;
    read_vec(e.get("origin"), at);
    const br::Model& bm = m.bsp().models[n];
    lo = sh::Vec3f{bm.mins[0], bm.mins[1], bm.mins[2]} + at;
    hi = sh::Vec3f{bm.maxs[0], bm.maxs[1], bm.maxs[2]} + at;
    angle = static_cast<float>(std::atof(e.get("angle").c_str()));
    return true;
  }
  return false;
}

const char* event_name(Event e) {
  return e == Event::plat    ? "plat"
         : e == Event::train ? "train"
         : e == Event::door  ? "door"
         : e == Event::push  ? "push"
                             : "water";
}

// The brush model number of the `index`th entity of `kind`; -1 none.
int model_of(const Map& m, const std::string& kind, int index) {
  int seen = 0;
  for (const ge::brush::Entity& e : m.bsp().entities)
    if (e.get("classname") == kind && seen++ == index && e.get("model").size() > 1)
      return std::atoi(e.get("model").c_str() + 1);
  return -1;
}

// Whether the player's box at `at` lies in empty cells (its middle and its eight corners).
bool open_box(const Map& m, sh::Vec3f at) {
  for (int i = 0; i < 9; i++) {
    const float p[3] = {at.x + (i == 8         ? 0
                                : (i & 1) != 0 ? 15.f
                                               : -15.f),
                        at.y + (i == 8         ? 0
                                : (i & 2) != 0 ? 15.f
                                               : -15.f),
                        at.z + (i == 8         ? 0
                                : (i & 4) != 0 ? 31.f
                                               : -23.f)};
    if (ge::brush::cell_at(m.bsp(), p) != ge::brush::Cell::empty) return false;
  }
  return true;
}

// The drives tried for the event, best first; the first the server agrees happened is taken.
std::vector<Drive> drives_for(const Map& m, Event ev, int index, std::string& why) {
  std::vector<Drive> out;
  sh::Vec3f lo, hi;
  float angle = 0;
  if (ev == Event::water) {
    std::vector<std::pair<float, std::pair<sh::Vec3f, sh::Vec3f>>> bodies;
    for (const ge::brush::LiquidBody& b : ge::brush::liquid_bodies(m.bsp())) {
      if (b.kind != ge::brush::Cell::water) continue;
      const sh::Vec3f bl{b.mins[0], b.mins[1], b.mins[2]}, bh{b.maxs[0], b.maxs[1], b.maxs[2]};
      bodies.push_back({(bh.x - bl.x) * (bh.y - bl.y), {bl, bh}});
    }
    std::sort(bodies.begin(), bodies.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    if (index >= static_cast<int>(bodies.size())) {
      why = "the map has " + std::to_string(bodies.size()) + " bodies of water";
      return out;
    }
    const auto& [bl, bh] = bodies[static_cast<size_t>(index)].second;
    for (int i = 1; i < 8; i++)
      for (int j = 1; j < 8; j++) {
        Drive d;
        d.start = {bl.x + (bh.x - bl.x) * static_cast<float>(i) / 8,
                   bl.y + (bh.y - bl.y) * static_cast<float>(j) / 8, bh.z + 40};
        d.forward = 400;
        d.up = 400;
        d.jump = true;
        d.seconds = 5;
        out.push_back(d);
      }
    return out;
  }
  const std::string kind = ev == Event::plat    ? "func_plat"
                           : ev == Event::train ? "func_train"
                           : ev == Event::door  ? "func_door"
                                                : "trigger_push";
  if (!site_of(m, kind, index, lo, hi, angle)) {
    why = "no " + kind + " " + std::to_string(index) + " on the map";
    return out;
  }
  const sh::Vec3f mid = (lo + hi) * 0.5f;
  if (ev == Event::plat || ev == Event::train) {
    Drive d;
    d.start = {mid.x, mid.y, hi.z + 40};
    d.model = model_of(m, kind, index);
    d.seconds = ev == Event::plat ? 6 : 8;
    out.push_back(d);
    return out;
  }
  // A door or a push: walked into, from open floor round it (the player's box in empty cells),
  // nearest first, at eight headings.
  const float z = (ev == Event::door ? lo.z : std::fmax(lo.z, mid.z - 40)) + 25;
  for (const float dist : {96.f, 160.f, 224.f, 320.f}) {
    for (int h = 0; h < 8; h++) {
      const float yaw = static_cast<float>(h) * 45.0f;
      const sh::Vec3f fwd = sh::angle_vectors({0, yaw, 0}).forward;
      const sh::Vec3f at{mid.x - fwd.x * (dist + (hi.x - lo.x + hi.y - lo.y) * 0.25f),
                         mid.y - fwd.y * (dist + (hi.x - lo.x + hi.y - lo.y) * 0.25f), z};
      if (!open_box(m, at)) continue;
      Drive d;
      d.start = at;
      d.aim = mid;
      d.lo = lo;
      d.hi = hi;
      d.yaw = yaw;
      d.forward = 400;
      d.seconds = 3;
      out.push_back(d);
      if (out.size() >= 12) return out;
    }
  }
  return out;
}

// Did the drive meet the event (judged off the server's own run without the fix)?
bool met(Event ev, const Run& r) {
  switch (ev) {
    case Event::plat:
      return r.rose > 32;
    case Event::train:
      return r.moved > 64;
    case Event::door:
      return r.inside && r.moved > 200;  // through the door's own box
    case Event::push:
      return r.rose > 48 || r.moved > 400;
    case Event::water:
      return r.waterlevel >= 2;
  }
  return false;
}

void run(gl::Lab& lab) {
  const int fps = static_cast<int>(lab.arg_num("fps"));
  const int index = static_cast<int>(lab.arg_num("index"));
  const std::string ev_name = lab.arg("event");
  Event ev = Event::plat;
  bool known = false;
  for (const Event e : {Event::plat, Event::train, Event::door, Event::push, Event::water})
    if (ev_name == event_name(e)) {
      ev = e;
      known = true;
    }
  std::vector<double> latencies;
  const std::string list = lab.arg("latencies");
  for (size_t at = 0; at < list.size();) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    latencies.push_back(std::atof(list.substr(at, comma - at).c_str()));
    at = comma + 1;
  }
  if (!known || fps < 30 || latencies.empty()) {
    lab.fail(
        "se-control: --event must be plat, train, door, push or water, --fps 30 or more "
        "and --latencies name one or more",
        "next: make lab LAB=se-control ARGS=--help");
    return;
  }
  const std::string basedir = lab.arg("basedir");
  if (basedir.empty()) {
    lab.note(
        "se-control measures a real map's movers only: pass --basedir=<the folder that holds the "
        "game folder> --game=<it> --map=<name>");
    return;
  }
  Map m;
  std::string error;
  cl::SearchPath files;
  m.name = lab.arg("map");
  const std::string name = m.name;
  const std::string install = basedir + "/" + lab.arg("game");
  std::error_code ec;
  if (std::filesystem::is_directory(install + "/baseq2", ec)) {  // a Quake II install
    const pf::GameProfile& game = *pf::find_game("q2");
    const ge::classicenhance::Profile& content = pf::content_of(game);
    if (!cl::open_game_dir(install, std::string(content.base_dir), files, error, content)) {
      lab.fail("se-control: " + error,
               "Pass --basedir and --game naming a Quake II install. Skill: net.");
      return;
    }
    std::unique_ptr<cl::LoadedMap> lm = cl::load_named_map(files, name, error);
    auto q2 = std::make_shared<br::Map>();
    if (!lm || !br::read_bsp(lm->file, q2.get(), &error)) {
      lab.fail("se-control: " + error, "Name a map the install holds (--map). Skill: net.");
      return;
    }
    m.q2 = q2;
  } else {
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail("se-control: " + error,
               "Pass --basedir=<the folder that holds the game folder>. Skill: net.");
      return;
    }
    m.loaded = cl::load_named_map(files, name, error);
    if (!m.loaded) {
      lab.fail("se-control: " + error, "Name a map the game folder holds (--map). Skill: net.");
      return;
    }
    m.spec = m.loaded->spec;
  }
  std::string why;
  const std::vector<Drive> drives = drives_for(m, ev, index, why);
  Drive chosen;
  bool found = false;
  std::string tried;
  for (const Drive& d : drives) {
    const Run probe = drive(m, d, 0, false, fps);
    if (tried.size() < 600)
      tried += " (" + to_fixed(d.start.x, 0) + " " + to_fixed(d.start.y, 0) + " " +
               to_fixed(d.start.z, 0) + "): rose " + to_fixed(probe.rose, 0) + ", moved " +
               to_fixed(probe.moved, 0) + ", nearest " + to_fixed(probe.nearest, 0) + ", water " +
               std::to_string(probe.waterlevel) + ";";
    if (probe.ok && met(ev, probe)) {
      chosen = d;
      found = true;
      break;
    }
  }
  if (!found) {
    lab.fail("se-control: no " + ev_name + " " + std::to_string(index) + " on " + name +
                 " the player meets" + (why.empty() ? "" : ": " + why) + "; tried" + tried,
             "Name another (--index) or read the drives in games/shooters_edge/labs/"
             "se_control_lab.cpp. Skill: net.");
    return;
  }
  const std::vector<std::pair<std::string, std::string>> in = {{"basedir", basedir},
                                                               {"map", name},
                                                               {"event", ev_name},
                                                               {"index", lab.arg("index")},
                                                               {"latencies", list}};
  gl::LabTable t{
      ev_name + " " + std::to_string(index) + " of " + name + ", from (" +
          to_fixed(chosen.start.x, 0) + " " + to_fixed(chosen.start.y, 0) + " " +
          to_fixed(chosen.start.z, 0) + ") facing " + to_fixed(chosen.yaw, 0),
      {"one way ms", "prediction", "worst miss (units)", "yanks (frames)", "pulled in all (units)"},
      {}};
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"JOINED TO A SERVER: WHERE THE SERVER MOVED THE PREDICTION, " + ev_name,
             name + ", the window at " + std::to_string(fps) + " fps", "since he set off (ms)",
             "the server's word against the prediction (units)"};
  for (double ms : latencies) {
    for (const bool foreseen : {true, false}) {
      const Run r = drive(m, chosen, ms, foreseen, fps);
      if (!r.ok) {
        lab.fail("se-control: at " + to_fixed(ms, 0) + " ms the window never joined",
                 "next: build/debug/tests/shooters_edge_remote_test. Skill: net.");
        return;
      }
      const char* way = foreseen ? "foreseen" : "waited";
      const std::string key = "ms-" + to_fixed(ms, 0) + "." + way;
      t.rows.push_back({to_fixed(ms, 0), way, to_fixed(r.worst, 1), std::to_string(r.yanks),
                        to_fixed(r.pulled, 1)});
      lab.metric(key + ".worst-miss", r.worst, "units");
      lab.metric(key + ".yanks", r.yanks, "frames");
      lab.metric(key + ".pulled", r.pulled, "units");
      c.traces.push_back({to_fixed(ms, 0) + " ms " + way, r.t, r.corr});
    }
  }
  lab.table(t);
  sl::shoot(lab, "correction", gl::line_chart(c),
            "Each window frame from the moment he sets off: how far a frame from the server moved "
            "the window's prediction (where it thought he was against where the server's word "
            "put him), before any smoothing. A spike is a yank; flat at 0 is full control.",
            "the 'foreseen' lines flat near 0; any line that rises is a moment the window "
            "disagreed with the server",
            in);
}

const gl::LabSpec kSpec{
    "se-control",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the joined window's prediction against the QuakeWorld or Quake II server through a real "
    "map's plats, "
    "trains, doors, push triggers and water, over the simulated network",
    "net",
    "Where does the window joined to a server lag or yank the player: riding a plat or a train, "
    "running through a door, thrown by a push trigger, in and out of water?",
    {
        {"fps", "144", "the window's frames a second (the server runs 77)"},
        {"latencies", "0,25,50", "one-way latencies, ms, two drives each (foreseen, waited)"},
        {"basedir", "", "the folder that holds the game folder (\"\": skipped)"},
        {"game", "", "the game folder under it (a Quake II install: the folder holding baseq2)"},
        {"map", "dm2", "the map"},
        {"event", "plat", "plat, train, door, push or water"},
        {"index", "0", "which of the map's entities of that kind (water: bodies, largest first)"},
    },
    "--latencies=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
