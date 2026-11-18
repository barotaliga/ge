// LAB `se-bhop`: HOW WELL DOES EACH BOT BUNNY HOP, SECTION BY SECTION, against how a person does
// it? The hop course (labs/hop_course.hpp: a run-up, a long straight, S-bends, corners of three
// radii, a U-turn, a corridor that narrows) is compiled from its plan in the run, played on our
// own server with the baseline mod, and ONE BOT of each skill (`--skills`) runs it from the spawn
// to the end as in a match: the whole fighter (bots/fighter.hpp: the pilot, the mouse hand, the
// keyboard hand, every switch of its skill's preset and `--features` over it), sent to the
// course's end as an objective set from outside (`Fighter::objective`). Nothing else is on the
// map: no item, no enemy, so the hop is all there is to watch. Every seed (`--seeds`) is another
// run of each skill (the hands' draws).
//
// THE READING, off the server's frames (the player's origin, velocity and view, the command's
// keys): each frame's place along the course (`course_station`) and its section; the hops by the
// same reader as people's recordings (shooter/hops.hpp `read_hops`: takeoffs, landings, the
// ground between); and by section and for the whole run:
// - the time, the speed in, out and on average;
// - the TAKEOFFS (a hop's first frame off the ground) and their speed, the GAIN a flight (the speed
//   at the landing less at the takeoff), the share of landings jumped on the landing frame;
// - the WALLS met: a frame losing more than 40 units/s and a tenth of its speed, not a landing's
//   (one every 0.2 s at most);
// - the HOP'S HANDS IN THE AIR (the technique the maintainer hops with, M18r.md 0b): the share of
//   the air with FORWARD held (none in a hop's air), the strafe key's REVERSALS a second, the
//   MOUSE WITH THE KEY (the share of the air the view turns toward the side whose key is held) and
//   the LEAN (the view off the velocity's heading, positive toward the held key's side) at each
//   reversal and over the air.
// People's yardsticks are stated beside them (dm2's recordings, se-moves and se-recipes: takeoffs
// p50 341 and p75 401, 46 % of takeoffs at 400 or more, +26..+62 units/s a flight, the jump on the
// landing frame).
//
// What it shows: the runs' table (a row a skill, the seeds' means), each skill's sections, the
// course from above with each skill's line coloured by its speed, and the speed along the course.
// `--trace=<tsv>` writes every frame of every run; `--write-plan=<file>` the course's plan, to
// build it with `secli map build` and hop it by hand. Deterministic: the digest holds every run.
//
// ge:summary Lab: bots of each skill bunny hop a practice course, read section by section
// ge:keywords lab se-bhop bunny-hop bhop hop-course practice-map takeoff gain walls reversals
// ge:keywords lean mouse-with-key forward-in-air sections technique drive
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/features.hpp"
#include "shooter/hops.hpp"
#include "shooter/nav.hpp"
#include "shooter/pilot.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/hop_course.hpp"
#include "shooters_edge/net/classic_team_nav.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace la = shooters_edge::labs;
namespace bt = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace mc = ge::brush::mapc;
using ge::core::num::to_fixed;

constexpr double kHz = 1000.0 / bt::kPilotMsec;
constexpr double kDt = bt::kPilotMsec / 1000.0;
constexpr float kWallLoss = 40;      // a frame losing this much speed or more ...
constexpr float kWallShare = 0.1f;   // ... and this share of it met a wall
constexpr double kWallGap_s = 0.2;   // one wall at most this often
constexpr float kArrive = 96;        // the run ends this near the course's end, units
constexpr float kFastTakeoff = 400;  // a fast takeoff, units/s (people: 46 % of theirs)

struct Frame {
  double t = 0;
  sh::Vec3f o, v;
  float yaw = 0, speed = 0, along = 0;
  int section = 0;
  int16_t forward = 0, side = 0;
  bool jump = false;
  uint8_t pilot = 0;
  float aim_yaw = 0;    // the hop's aim (the pilot's `hop_aim_yaw`)
  int8_t hop_side = 0;  // the side the pilot holds (1 right)
  int8_t air_set = -1;  // the air's key set (bot.cpp; -1 none)
  const char* view_by = "";
  const char* keys_by = "";
};

// What a stretch of frames (a section, or the whole run) shows.
struct Reading {
  double time_s = 0, air_s = 0;
  float speed_in = 0, speed_out = 0;
  double speed_sum = 0;
  int64_t frames = 0, air_frames = 0, air_forward = 0, air_with_key = 0, air_keyed = 0;
  int64_t hop_frames = 0;
  int64_t walls = 0, reversals = 0, landings = 0, landing_jumps = 0;
  std::vector<float> takeoffs, gains, leans, lean_rev;
  bool started = false;
};

float median(std::vector<float> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}
float pct(std::vector<float> v, double q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size())))];
}
double share(int64_t a, int64_t b) {
  return b > 0 ? static_cast<double>(a) / static_cast<double>(b) : 0;
}

struct Run {
  sh::AimSkill skill = sh::AimSkill::expert;
  uint32_t seed = 1;
  bool arrived = false;
  double lap_s = 0;
  std::vector<Frame> frames;
  std::vector<sh::Vec3f> route;  // the pilot's route's nodes as first planned (the nav's line)
  sh::HopReading hops;
  Reading all;
  std::vector<Reading> sections;
  std::string error;
};

// One bot of `skill` down the course on the server, every frame kept.
Run run_course(std::span<const uint8_t> bsp, const la::HopCourse& course, sh::AimSkill skill,
               uint32_t seed, const std::string& features, double limit_s) {
  Run r;
  r.skill = skill;
  r.seed = seed;
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(bsp, "hop_course", r.error);
  if (!map) return r;
  ge::progs::Progs progs;
  if (!sv::baseline_mod(progs, r.error)) return r;
  sv::QcServerOptions so;
  so.seed = seed;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> sp = sv::make_qc_server(progs, so, r.error);
  if (!sp) return r;
  sv::QcServer& s = *sp;
  sv::server_command(s, "deathmatch 1");
  if (!sv::load_map(s, map->spec, r.error)) return r;
  cn::TeamNav tn;
  if (!bt::build_map_nav(s, map->spec, "info_player_deathmatch", tn.nav, r.error)) return r;
  const sh::NavGraph& g = tn.nav.graph;
  const sh::Vec3f end{course.mx.back(), course.my.back(), 24};
  const int32_t goal = sh::nav_nearest(g, end);
  if (goal < 0) {
    r.error = "no nav node near the course's end";
    return r;
  }

  sv::connect(s, 0, "hopper", false, true);
  bt::Fighter f;
  f.features = sh::features_of(skill);
  if (!sh::apply_features(features, f.features, r.error)) return r;
  const sh::Traits traits = sh::traits_of(skill, sh::Temper::balanced);
  cn::team_fighter_setup(tn, f, s, 0, map->spec.entities.c_str(), skill, traits, kHz, seed,
                         "hopper");

  sh::Packet packet;
  sh::MoveCmd recent[3] = {};
  const int frames = static_cast<int>(limit_s * kHz);
  for (int frame = 0; frame < frames; frame++) {
    f.objective = goal;
    const sh::MoveCmd cmd = bt::fighter_command(f, s, 0, bt::kPilotMsec);
    recent[0] = recent[1];
    recent[1] = recent[2];
    recent[2] = cmd;
    packet.client = 0;
    packet.sequence++;
    for (int i = 0; i < 3; i++) packet.cmds[i] = recent[i];
    s.sv.sound_count = 0;
    sv::qc_frame(s, &packet, 1);
    sv::clear_frame(s.out);
    const sh::Edict& e = s.sv.edicts[1];
    Frame fr;
    fr.t = (frame + 1) * kDt;
    fr.o = e.origin;
    fr.v = e.velocity;
    fr.yaw = cmd.angles.y;
    fr.speed = std::sqrt(e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y);
    fr.along = la::course_station(course, e.origin.x, e.origin.y, &fr.section);
    fr.forward = cmd.forward;
    fr.side = cmd.side;
    fr.jump = (cmd.buttons & sh::kButtonJump) != 0;
    fr.pilot = static_cast<uint8_t>(f.bot.pilot.pilot.mode);
    fr.aim_yaw = f.bot.pilot.pilot.hop_aim_yaw;
    if (r.route.empty())
      for (const int32_t n : f.bot.pilot.pilot.route)
        r.route.push_back(g.nodes[static_cast<size_t>(n)].origin);
    fr.hop_side = f.bot.pilot.pilot.strafe_side;
    fr.air_set = static_cast<int8_t>(f.bot.air_set);
    fr.view_by = f.bot.view_by ? f.bot.view_by : "";
    fr.keys_by = f.keys_by ? f.keys_by : "";
    r.frames.push_back(fr);
    const float dx = e.origin.x - end.x, dy = e.origin.y - end.y;
    if (dx * dx + dy * dy < kArrive * kArrive) {
      r.arrived = true;
      r.lap_s = fr.t;
      break;
    }
    if (s.map_stopped) {
      r.error = "the mod stopped the map: " + s.stop_reason;
      return r;
    }
  }
  if (!r.arrived) r.lap_s = limit_s;
  return r;
}

// The run read: the hops by people's reader, then every frame into its section and the whole.
void read_run(Run& r, const sh::MoveWorld& world, const la::HopCourse& course) {
  std::vector<sh::TrackPoint> track;
  track.reserve(r.frames.size());
  for (const Frame& f : r.frames) track.push_back({f.t, f.o, f.yaw, 0, false});
  sh::HopSettings hs;
  hs.cmd_hz = kHz;
  sh::read_hops(track, world, hs, r.hops);
  r.sections.assign(course.sections.size(), Reading{});
  const std::vector<sh::HopFrame>& hf = r.hops.frames;
  double last_wall = -1e9;
  int side_was = 0;
  auto both = [&](size_t i, auto&& fn) {
    fn(r.all);
    fn(r.sections[static_cast<size_t>(r.frames[i].section)]);
  };
  for (size_t i = 0; i < r.frames.size(); i++) {
    const Frame& f = r.frames[i];
    const bool air = i < hf.size() && hf[i].phase == sh::HopPhase::air;
    const bool landed = i > 0 && i < hf.size() && hf[i].phase == sh::HopPhase::ground &&
                        hf[i - 1].phase == sh::HopPhase::air;
    both(i, [&](Reading& g) {
      if (!g.started) {
        g.started = true;
        g.speed_in = f.speed;
      }
      g.speed_out = f.speed;
      g.frames++;
      g.time_s += kDt;
      g.speed_sum += f.speed;
      if (f.pilot == static_cast<uint8_t>(sh::PilotMode::hop)) g.hop_frames++;
    });
    if (i > 0) {
      const float was = r.frames[i - 1].speed, loss = was - f.speed;
      if (!landed && loss > kWallLoss && loss > kWallShare * was && f.t - last_wall > kWallGap_s) {
        last_wall = f.t;
        both(i, [&](Reading& g) { g.walls++; });
      }
    }
    if (air && f.speed > 100) {
      const int side = f.side > 0 ? 1 : f.side < 0 ? -1 : 0;
      // The lean: the view off the velocity, + toward the held key's side (the right key turns
      // the view clockwise, to negative yaw).
      const float lean = side != 0 ? hf[i].lag * static_cast<float>(-side) : 0;
      float view_turn = 0;
      if (i > 0) {
        view_turn = f.yaw - r.frames[i - 1].yaw;  // degrees, + to the left
        if (view_turn > 180) view_turn -= 360;
        if (view_turn < -180) view_turn += 360;
      }
      const bool reversed = side != 0 && side_was != 0 && side != side_was;
      both(i, [&](Reading& g) {
        g.air_frames++;
        g.air_s += kDt;
        if (f.forward > 0) g.air_forward++;
        if (side != 0) {
          g.air_keyed++;
          g.leans.push_back(lean);
          if (view_turn * static_cast<float>(-side) > 0) g.air_with_key++;
        }
        if (reversed) {
          g.reversals++;
          g.lean_rev.push_back(lean);
        }
      });
      if (side != 0) side_was = side;
    }
    if (!air) side_was = 0;
  }
  for (const sh::Hop& h : r.hops.hops) {
    const size_t i = static_cast<size_t>(h.takeoff);
    if (i >= r.frames.size()) continue;
    both(i, [&](Reading& g) {
      g.takeoffs.push_back(h.speed_off);
      g.gains.push_back(h.speed_land - h.speed_off);
      if (h.ground_before >= 0) {
        g.landings++;
        if (h.ground_before <= 1) g.landing_jumps++;
      }
    });
  }
}

gl::Rgba speed_colour(float v) {
  // 0 blue, 320 cyan, 500 green, 750 yellow, 1000+ red.
  const float at[5] = {0, 320, 500, 750, 1000};
  const float stops[5][3] = {
      {40, 60, 200}, {40, 170, 220}, {60, 190, 80}, {240, 200, 40}, {220, 40, 40}};
  const float x = std::clamp(v, 0.0f, 1000.0f);
  int a = 0;
  while (a < 3 && x > at[a + 1]) a++;
  const float t = (x - at[a]) / (at[a + 1] - at[a]);
  auto mix = [&](int c) {
    return static_cast<uint8_t>(stops[a][c] + (stops[a + 1][c] - stops[a][c]) * t);
  };
  return {mix(0), mix(1), mix(2)};
}

struct View {
  float x0 = 0, y0 = 0, scale = 1;
  int w = 0, h = 0;
  double px(float x) const { return (x - x0) * scale; }
  double py(float y) const { return static_cast<float>(h) - (y - y0) * scale; }
};

gl::Picture course_picture(const la::HopCourse& c, View& v) {
  float lx = 1e9f, ly = 1e9f, hx = -1e9f, hy = -1e9f;
  for (const la::HopQuad& q : c.quads)
    for (int k = 0; k < 4; k++) {
      lx = std::min(lx, q.x[k]);
      ly = std::min(ly, q.y[k]);
      hx = std::max(hx, q.x[k]);
      hy = std::max(hy, q.y[k]);
    }
  constexpr float kPad = 64;
  v.w = 1400;
  v.scale = static_cast<float>(v.w) / (hx - lx + 2 * kPad);
  v.h = static_cast<int>((hy - ly + 2 * kPad) * v.scale);
  v.x0 = lx - kPad;
  v.y0 = ly - kPad;
  gl::Picture p = gl::create_picture(v.w, v.h, {28, 28, 32});
  for (const la::HopQuad& q : c.quads) {
    float qx0 = 1e9f, qy0 = 1e9f, qx1 = -1e9f, qy1 = -1e9f;
    for (int k = 0; k < 4; k++) {
      qx0 = std::min(qx0, q.x[k]);
      qy0 = std::min(qy0, q.y[k]);
      qx1 = std::max(qx1, q.x[k]);
      qy1 = std::max(qy1, q.y[k]);
    }
    const gl::Rgba floor = q.section % 2 ? gl::Rgba{92, 92, 100} : gl::Rgba{110, 110, 118};
    for (int py = static_cast<int>(v.py(qy1)); py <= static_cast<int>(v.py(qy0)) + 1; py++)
      for (int px = static_cast<int>(v.px(qx0)); px <= static_cast<int>(v.px(qx1)) + 1; px++) {
        const float wx = v.x0 + static_cast<float>(px) / v.scale;
        const float wy = v.y0 + static_cast<float>(v.h - py) / v.scale;
        bool in = true;
        for (int k = 0; k < 4 && in; k++) {
          const int n = (k + 1) % 4;
          in = (q.x[n] - q.x[k]) * (wy - q.y[k]) - (q.y[n] - q.y[k]) * (wx - q.x[k]) >= 0;
        }
        if (in) gl::set_pixel(p, px, py, floor);
      }
  }
  // The sections' names at their starts (a point 40 units in, along the middle).
  for (size_t k = 0; k < c.sections.size(); k++) {
    const float d = c.section_from[k] + 40;
    size_t i = 1;
    while (i + 1 < c.at.size() && c.at[i] < d) i++;
    const float f =
        std::clamp((d - c.at[i - 1]) / std::max(1.0f, c.at[i] - c.at[i - 1]), 0.0f, 1.0f);
    const float tx = c.mx[i - 1] + (c.mx[i] - c.mx[i - 1]) * f;
    const float ty = c.my[i - 1] + (c.my[i] - c.my[i - 1]) * f;
    const double x =
        std::min(v.px(tx) + 4, static_cast<double>(v.w - gl::text_width(c.sections[k], 2) - 4));
    gl::draw_text(p, x, v.py(ty) - 4, c.sections[k], {230, 230, 230}, 2);
  }
  return p;
}

void legend(gl::Picture& p) {
  // A cell every 20 units/s, 0 to 1000.
  for (int k = 0; k <= 50; k++)
    gl::fill_rect(p, 20 + k * 6, 20, 6, 14, speed_colour(static_cast<float>(k * 20)));
  gl::draw_text(p, 20, 40, "0", {230, 230, 230}, 2);
  gl::draw_text(p, 20 + 16 * 6 - 12, 40, "320", {230, 230, 230}, 2);
  gl::draw_text(p, 20 + 25 * 6 - 6, 40, "500", {230, 230, 230}, 2);
  gl::draw_text(p, 20 + 37 * 6 - 6, 40, "750", {230, 230, 230}, 2);
  gl::draw_text(p, 20 + 50 * 6 - 12, 40, "1000 U/S", {230, 230, 230}, 2);
}

std::vector<uint32_t> parse_seeds(const std::string& s) {
  std::vector<uint32_t> out;
  size_t at = 0;
  while (at < s.size()) {
    const size_t end = std::min(s.find(',', at), s.size());
    const std::string w = s.substr(at, end - at);
    const size_t dash = w.find('-');
    if (dash == std::string::npos) {
      out.push_back(static_cast<uint32_t>(std::stoul(w)));
    } else {
      const uint32_t a = static_cast<uint32_t>(std::stoul(w.substr(0, dash)));
      const uint32_t b = static_cast<uint32_t>(std::stoul(w.substr(dash + 1)));
      for (uint32_t k = a; k <= b; k++) out.push_back(k);
    }
    at = end + 1;
  }
  return out;
}

void run(gl::Lab& lab) {
  const la::HopCourse course = la::make_hop_course();
  const std::string plan = la::hop_course_plan(course);
  if (!lab.arg("write-plan").empty()) {
    std::ofstream out(lab.arg("write-plan"));
    out << plan;
    if (!out) {
      lab.fail("se-bhop: cannot write --write-plan=" + lab.arg("write-plan"),
               "next: pass a path this user may write");
      return;
    }
    lab.note("the course's plan written to " + lab.arg("write-plan"));
  }
  mc::Source src;
  std::string err;
  if (!mc::read_plan(plan, &src, &err)) {
    lab.fail("se-bhop: the course's plan does not read:\n" + err,
             "next: fix games/shooters_edge/labs/hop_course.hpp hop_course_plan");
    return;
  }
  mc::CompileOptions opt;
  opt.light = false;
  mc::CompileReport rep;
  std::vector<uint8_t> bsp;
  if (!mc::compile_map(src, opt, &bsp, &rep, &err)) {
    lab.fail("se-bhop: the course does not compile: " + err,
             "next: fix the pieces in games/shooters_edge/labs/hop_course.hpp");
    return;
  }
  lab.note("the course: " + std::to_string(course.quads.size()) + " rooms, " +
           to_fixed(course.length, 0) + " units along its middle, " +
           std::to_string(course.sections.size()) + " sections; compiled to " +
           std::to_string(rep.brushes) + " brushes, " + std::to_string(rep.faces) + " faces");

  // The world the reader traces the ground in.
  std::unique_ptr<cl::LoadedMap> loaded = cl::map_from_bytes(bsp, "hop_course", err);
  if (!loaded) {
    lab.fail("se-bhop: the course does not load: " + err, "next: run tests/brush_compile_test");
    return;
  }
  sh::MoveWorld world;
  sh::map_world(loaded->spec.models[0], world);

  std::vector<sh::AimSkill> skills;
  {
    const std::string list = lab.arg("skills");
    size_t at = 0;
    while (at < list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      const std::string w = list.substr(at, end - at);
      bool found = false;
      for (const sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::average,
                                   sh::AimSkill::expert, sh::AimSkill::perfect})
        if (w == sh::aim_skill_name(k)) skills.push_back(k), found = true;
      if (!found) {
        lab.fail("se-bhop: --skills: '" + w + "' is no skill",
                 "next: novice, average, expert or perfect, comma separated");
        return;
      }
      at = end + 1;
    }
  }
  const std::vector<uint32_t> seeds = parse_seeds(lab.arg("seeds"));
  const double limit_s = lab.arg_num("limit-s");
  const std::string features = lab.arg("features");

  std::vector<Run> runs;
  for (const sh::AimSkill k : skills)
    for (const uint32_t seed : seeds) {
      runs.push_back(run_course(bsp, course, k, seed, features, limit_s));
      Run& r = runs.back();
      if (!r.error.empty()) {
        lab.fail("se-bhop: " + std::string(sh::aim_skill_name(k)) + " seed " +
                     std::to_string(seed) + ": " + r.error,
                 "next: run the se-team lab on the fixture halls to see the fighter run at all");
        return;
      }
      read_run(r, world, course);
    }

  if (!lab.arg("trace").empty()) {
    std::ofstream t(lab.arg("trace"));
    t << "skill\tseed\tt\tx\ty\tz\tspeed\talong\tsection\tphase\tforward\tside\tjump\tyaw"
         "\theading\tlag\tpilot\tview_by\tkeys_by\taim\thop_side\tair_set\n";
    for (const Run& r : runs)
      for (size_t i = 0; i < r.frames.size(); i++) {
        const Frame& f = r.frames[i];
        const sh::HopFrame& h = r.hops.frames[i];
        t << sh::aim_skill_name(r.skill) << '\t' << r.seed << '\t' << to_fixed(f.t, 3) << '\t'
          << to_fixed(f.o.x, 1) << '\t' << to_fixed(f.o.y, 1) << '\t' << to_fixed(f.o.z, 1) << '\t'
          << to_fixed(f.speed, 1) << '\t' << to_fixed(f.along, 0) << '\t'
          << course.sections[static_cast<size_t>(f.section)] << '\t'
          << (h.phase == sh::HopPhase::air      ? "air"
              : h.phase == sh::HopPhase::ground ? "ground"
                                                : "cut")
          << '\t' << f.forward << '\t' << f.side << '\t' << (f.jump ? 1 : 0) << '\t'
          << to_fixed(f.yaw, 1) << '\t' << to_fixed(h.heading, 1) << '\t' << to_fixed(h.lag, 1)
          << '\t' << sh::pilot_mode_name(static_cast<sh::PilotMode>(f.pilot)) << '\t' << f.view_by
          << '\t' << f.keys_by << '\t' << to_fixed(f.aim_yaw, 1) << '\t'
          << static_cast<int>(f.hop_side) << '\t' << static_cast<int>(f.air_set) << '\n';
      }
    if (!t) {
      lab.fail("se-bhop: cannot write --trace=" + lab.arg("trace"),
               "next: pass a path this user may write");
      return;
    }
  }

  // The runs' table: a row a skill, its seeds' means; the metrics beside it.
  gl::LabTable tab{"Each skill down the hop course (the seeds' means; people's yardsticks below)",
                   {"skill", "arrived", "lap s", "speed", "takeoff p50", "p75", "400+", "gain p50",
                    "landing jumps", "walls", "fwd in air", "no key in air", "reversals/s",
                    "mouse with key", "lean", "lean at rev", "hop share"},
                   {}};
  for (const sh::AimSkill k : skills) {
    double arrived = 0, lap = 0, speed = 0, walls = 0, revs = 0, n = 0;
    Reading pool;
    for (const Run& r : runs) {
      if (r.skill != k) continue;
      n++;
      arrived += r.arrived ? 1 : 0;
      lap += r.lap_s;
      speed += r.all.frames > 0 ? r.all.speed_sum / static_cast<double>(r.all.frames) : 0;
      walls += static_cast<double>(r.all.walls);
      revs += r.all.air_s > 0 ? static_cast<double>(r.all.reversals) / r.all.air_s : 0;
      pool.takeoffs.insert(pool.takeoffs.end(), r.all.takeoffs.begin(), r.all.takeoffs.end());
      pool.gains.insert(pool.gains.end(), r.all.gains.begin(), r.all.gains.end());
      pool.leans.insert(pool.leans.end(), r.all.leans.begin(), r.all.leans.end());
      pool.lean_rev.insert(pool.lean_rev.end(), r.all.lean_rev.begin(), r.all.lean_rev.end());
      pool.landings += r.all.landings;
      pool.landing_jumps += r.all.landing_jumps;
      pool.air_frames += r.all.air_frames;
      pool.air_forward += r.all.air_forward;
      pool.air_keyed += r.all.air_keyed;
      pool.air_with_key += r.all.air_with_key;
      pool.hop_frames += r.all.hop_frames;
      pool.frames += r.all.frames;
    }
    if (n == 0) continue;
    int64_t fast = 0;
    for (const float v : pool.takeoffs) fast += v >= kFastTakeoff ? 1 : 0;
    const std::string sk = sh::aim_skill_name(k);
    const double fast_share = share(fast, static_cast<int64_t>(pool.takeoffs.size()));
    tab.rows.push_back(
        {sk, to_fixed(arrived / n * 100, 0) + " %", to_fixed(lap / n, 2), to_fixed(speed / n, 0),
         to_fixed(median(pool.takeoffs), 0), to_fixed(pct(pool.takeoffs, 0.75), 0),
         to_fixed(fast_share * 100, 0) + " %", to_fixed(median(pool.gains), 0),
         to_fixed(share(pool.landing_jumps, pool.landings) * 100, 0) + " %", to_fixed(walls / n, 1),
         to_fixed(share(pool.air_forward, pool.air_frames) * 100, 0) + " %",
         to_fixed((1 - share(pool.air_keyed, pool.air_frames)) * 100, 0) + " %",
         to_fixed(revs / n, 2), to_fixed(share(pool.air_with_key, pool.air_keyed) * 100, 0) + " %",
         to_fixed(median(pool.leans), 1), to_fixed(median(pool.lean_rev), 1),
         to_fixed(share(pool.hop_frames, pool.frames) * 100, 0) + " %"});
    const std::string m = "bhop." + sk + ".";
    lab.metric(m + "arrived-share", arrived / n, "share");
    lab.metric(m + "lap-s", lap / n, "s");
    lab.metric(m + "speed-mean", speed / n, "u/s");
    lab.metric(m + "takeoff-p50", median(pool.takeoffs), "u/s");
    lab.metric(m + "takeoff-p75", pct(pool.takeoffs, 0.75), "u/s");
    lab.metric(m + "takeoff-400-share", fast_share, "share");
    lab.metric(m + "gain-p50", median(pool.gains), "u/s");
    lab.metric(m + "landing-jump-share", share(pool.landing_jumps, pool.landings), "share");
    lab.metric(m + "walls", walls / n, "count");
    lab.metric(m + "air-forward-share", share(pool.air_forward, pool.air_frames), "share");
    lab.metric(m + "air-no-key-share", 1 - share(pool.air_keyed, pool.air_frames), "share");
    lab.metric(m + "reversals-per-s", revs / n, "1/s");
    lab.metric(m + "mouse-with-key-share", share(pool.air_with_key, pool.air_keyed), "share");
    lab.metric(m + "lean-p50", median(pool.leans), "deg");
    lab.metric(m + "lean-at-reversal-p50", median(pool.lean_rev), "deg");
    lab.metric(m + "hop-share", share(pool.hop_frames, pool.frames), "share");
  }
  tab.rows.push_back({"people (dm2)", "", "", "", "341", "401", "46 %", "+26..+62", "on landing",
                      "", "0 %", "", "", "", "", "", ""});
  lab.table(tab);

  // Each skill's sections (its first seed).
  for (const sh::AimSkill k : skills) {
    const Run* r = nullptr;
    for (const Run& x : runs)
      if (x.skill == k) {
        r = &x;
        break;
      }
    if (!r) continue;
    gl::LabTable st{std::string(sh::aim_skill_name(k)) + ", seed " + std::to_string(r->seed) +
                        ": section by section",
                    {"section", "time s", "speed in", "out", "mean", "takeoffs", "takeoff p50",
                     "gain p50", "landing jumps", "walls", "fwd in air", "reversals/s",
                     "mouse with key", "lean at rev", "hop share"},
                    {}};
    for (size_t s = 0; s < course.sections.size(); s++) {
      const Reading& g = r->sections[s];
      if (g.frames == 0) {
        st.rows.push_back(
            {course.sections[s], "never", "", "", "", "", "", "", "", "", "", "", "", "", ""});
        continue;
      }
      st.rows.push_back(
          {course.sections[s], to_fixed(g.time_s, 2), to_fixed(g.speed_in, 0),
           to_fixed(g.speed_out, 0), to_fixed(g.speed_sum / static_cast<double>(g.frames), 0),
           std::to_string(g.takeoffs.size()), to_fixed(median(g.takeoffs), 0),
           to_fixed(median(g.gains), 0),
           to_fixed(share(g.landing_jumps, g.landings) * 100, 0) + " %", std::to_string(g.walls),
           to_fixed(share(g.air_forward, g.air_frames) * 100, 0) + " %",
           to_fixed(g.air_s > 0 ? static_cast<double>(g.reversals) / g.air_s : 0, 2),
           to_fixed(share(g.air_with_key, g.air_keyed) * 100, 0) + " %",
           to_fixed(median(g.lean_rev), 1),
           to_fixed(share(g.hop_frames, g.frames) * 100, 0) + " %"});
    }
    lab.table(st);
  }

  // The course from above, a picture a skill: its first seed's line coloured by speed, its
  // takeoffs dotted white and its walls ringed red.
  const std::vector<std::pair<std::string, std::string>> in = {
      {"seeds", lab.arg("seeds")}, {"features", features.empty() ? "preset" : features}};
  for (const sh::AimSkill k : skills) {
    const std::string id = std::string("line-") + sh::aim_skill_name(k);
    if (!lab.wants(id)) continue;
    const Run* r = nullptr;
    for (const Run& x : runs)
      if (x.skill == k) {
        r = &x;
        break;
      }
    if (!r) continue;
    View v;
    gl::Picture p = course_picture(course, v);
    for (size_t i = 1; i < r->route.size(); i++)  // the nav's route, thin and pale under the line
      gl::draw_line(p, v.px(r->route[i - 1].x), v.py(r->route[i - 1].y), v.px(r->route[i].x),
                    v.py(r->route[i].y), {150, 170, 210}, 1);
    for (size_t i = 1; i < r->frames.size(); i++) {
      const Frame& a = r->frames[i - 1];
      const Frame& b = r->frames[i];
      gl::draw_line(p, v.px(a.o.x), v.py(a.o.y), v.px(b.o.x), v.py(b.o.y), speed_colour(b.speed),
                    3);
    }
    for (const sh::Hop& h : r->hops.hops) {
      const Frame& f = r->frames[static_cast<size_t>(h.takeoff)];
      gl::fill_disc(p, v.px(f.o.x), v.py(f.o.y), 2.5, {255, 255, 255});
    }
    double last_wall = -1e9;
    for (size_t i = 1; i < r->frames.size(); i++) {
      const float was = r->frames[i - 1].speed, loss = was - r->frames[i].speed;
      const bool landed = r->hops.frames[i].phase == sh::HopPhase::ground &&
                          r->hops.frames[i - 1].phase == sh::HopPhase::air;
      if (!landed && loss > kWallLoss && loss > kWallShare * was &&
          r->frames[i].t - last_wall > kWallGap_s) {
        last_wall = r->frames[i].t;
        gl::draw_ring(p, v.px(r->frames[i].o.x), v.py(r->frames[i].o.y), 9, {255, 60, 60});
      }
    }
    legend(p);
    lab.shot({id,
              std::string("THE HOP COURSE: ") + sh::aim_skill_name(k) + ", SEED " +
                  std::to_string(r->seed),
              "The practice course from above (sections shaded in turn and named at their "
              "starts), the bot's line coloured by its speed (blue slow, cyan 320, green 500, "
              "yellow 750, red 1000), each takeoff a white dot, each wall met a red ring; the "
              "nav's route a thin "
              "pale line under it. " +
                  std::string(r->arrived ? "Reached the end in " + to_fixed(r->lap_s, 2) + " s."
                                         : "Did not reach the end."),
              "the line warming from green to yellow and red down the straights; white dots evenly "
              "spaced (a jump every landing); no red rings",
              in},
             p);
  }

  if (lab.wants("speed")) {
    gl::LineChart ch;
    ch.spikes = false;
    ch.frame = {"SPEED ALONG THE HOP COURSE",
                "each skill's first seed",
                "distance along (u)",
                "speed (u/s)",
                1200,
                460};
    for (const sh::AimSkill k : skills)
      for (const Run& r : runs) {
        if (r.skill != k) continue;
        gl::Trace tr{sh::aim_skill_name(k), {}, {}};
        for (const Frame& f : r.frames) {
          tr.x.push_back(f.along);
          tr.y.push_back(f.speed);
        }
        ch.traces.push_back(tr);
        break;
      }
    for (size_t s = 1; s < course.sections.size(); s++)
      ch.marks.push_back({course.section_from[s], course.sections[s]});
    ch.rules = {320, 400};
    const gl::Chart c = gl::line_chart(ch);
    lab.shot({"speed", c.title,
              "Each skill's speed against where it is along the course's middle; the sections "
              "start at the marks, the dashed lines are a run's 320 and a fast takeoff's 400.",
              "the speed climbing hop by hop on the straights and kept round the bends", in},
             gl::to_picture(c.plot));
  }
}

const gl::LabSpec kSpec{
    "se-bhop",
    gl::LabKind::drive,
    "game:shooters_edge",
    "plugins/shooter/pilot.cpp's hop and plugins/shooter/bot.cpp's air view under the fighter's "
    "hands (games/shooters_edge/bots/fighter.hpp) on the hop course "
    "(games/shooters_edge/labs/hop_course.hpp), read by plugins/shooter/hops.hpp",
    "bots",
    "How well does each bot bunny hop a practice course, section by section, against people?",
    {
        {"skills", "expert,average,novice", "the bots run, one a skill, comma separated"},
        {"seeds", "1", "the runs of each skill: a list or a range (1-6)"},
        {"features", "", "over each skill's preset (shooter/features.hpp: -hop, +x)"},
        {"limit-s", "60", "a run gives up after this long, s"},
        {"trace", "", "write every run's every frame here (TSV)"},
        {"write-plan", "", "write the course's plan here, for secli map build"},
    },
    "--skills=expert --limit-s=45",
    run,
};
GE_LAB(kSpec);

}  // namespace
