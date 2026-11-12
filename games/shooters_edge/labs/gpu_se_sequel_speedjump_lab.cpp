// LAB `se-sequel-speedjump`: DOES THE EASY JUMP HOP AND GAIN ON THE SEQUEL'S MOVE? The easy jump
// (`cl_speedjump 1`; plugins/shooter/speedjump_assist.hpp) carried to the sequel's player move
// (plugins/shooter/sequel_move.hpp) by its own air law (`sequel_jump_rules`) and its own jump (the
// up move, which needs a let-go as the classic jump does), measured three ways.
//
// THE MOVE ALONE: commands of 13 ms through `sequel_move` on an endless floor, a second's run-up,
// then eight seconds of forward and jump held: without the aid (one jump, then the move's 300),
// and through the aid at shares 0.5, 0.8 (the window's own server's pm_speedjump) and 1, under
// both of the sequel's air laws: the server's air acceleration 0 (its default, and always in a
// single-player game: no cap, accelerate 1) and 1 (deathmatch with `sv_airaccelerate 1`: the
// classic cap of 30, accelerate 10). The paths from above with the aim held east, and the steer:
// the aim turned 90 and 180 degrees at 3 s (the slow air turns a far-off aim on foot).
//
// IN THE GAME: the window's listen server on a hall of brushes of our own (a map of brushes plays
// on the sequel's rules, net/classic_game.cpp), the player running forward for a second and then
// holding forward and jump, the view turning `--turn` degrees a second, once with `cl_speedjump 0`
// and once with `cl_speedjump 1` sent as his user info (as the window does), the speed read off
// the server's player each frame. With `--game=<folder>` (the folder holding the sequel's base
// folder of packs; private pictures only) the player is also put on `--map`'s longest open run and
// drawn from his eyes `--at` seconds into the easy run, at cl_speedjump_view 1 and 0.
//
// ge:summary Lab: the easy jump on the sequel's move: its air laws, its up-move jump, in the game
// ge:keywords lab se-sequel-speedjump speedjump cl_speedjump pm_speedjump easy-jump sequel
// ge:keywords sequel-move air-accelerate sv_airaccelerate bunny-hop strafe-jump listen-server
// ge:skill shooter labs
// ge:test tests/shooter_speedjump_assist_test.cpp
#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooter/pmove.hpp"
#include "shooter/sequel_move.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace ls = ge::lab::liquid_scene;
namespace pf = shooters_edge::profiles;
using ge::core::num::to_fixed;

constexpr int kRate = 77;       // commands a second, 13 ms each
constexpr int kSeconds = 8;     // of hands after the run-up
constexpr float kShare = 0.8f;  // the window's own server's pm_speedjump
constexpr double kPi = 3.14159265358979323846;

float plan_speed(sh::Vec3f v) { return sh::root_of(v.x * v.x + v.y * v.y); }
sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

// THE FIELD: an endless floor at z 0; a box's bottom stops 1/32 above it.
sh::SequelTrace floor_trace(void*, sh::Vec3f a, sh::Vec3f mins, sh::Vec3f, sh::Vec3f b) {
  sh::SequelTrace t;
  const float from = a.z + mins.z, to = b.z + mins.z;
  t.end = b;
  if (from < 0) {
    t.start_solid = t.all_solid = true;
    t.fraction = 0;
    t.end = a;
    return t;
  }
  if (to >= 0.03125f) return t;
  t.fraction = std::max(0.0f, (from - 0.03125f) / (from - to));
  t.end = a + (b - a) * t.fraction;
  t.normal = {0, 0, 1};
  t.entity = 0;
  return t;
}
int32_t floor_contents(void*, sh::Vec3f) { return 0; }

constexpr double kSpeedJumpNear = 20;  // degrees off the turned aim counted as there

struct MoveRun {
  gl::Trace speed, path;
  int jumps = 0;
  float end = 0;
  double reached = -1;  // seconds from the aim's turn to the way within kSpeedJumpNear of it
};

// A run on the field: a second's run-up east, then forward and jump held for kSeconds, through the
// aid at `share` (0: the hands alone), the server's air acceleration `air`; the aim turned `turn`
// degrees left at 3 s. The path is unwrapped across the packed origin's ±4096.
MoveRun move_run(float share, float air, const std::string& label, float turn = 0) {
  const sh::SequelWorld world{nullptr, &floor_trace, &floor_contents};
  sh::SequelMove pm;
  pm.s.origin[2] = 24 * 8;
  pm.air_accelerate = air;
  MoveRun r;
  r.speed.label = r.path.label = label;
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  sh::SpeedJumpAid aid;
  sh::Vec3f shift, last;
  for (int i = 0; i < (1 + kSeconds) * kRate; i++) {
    if (i == kRate) cmd.buttons = sh::kButtonJump;
    if (i == 3 * kRate) cmd.angles.y = turn;
    sh::MoveState s;
    s.velocity = unpack(pm.s.velocity);
    s.ground = pm.ground_entity >= 0 ? 0 : -1;
    const bool down = s.ground == 0;
    const sh::MoveCmd sent =
        share > 0 ? sh::assisted_command(cmd, s, share, 0.5f, aid, sh::sequel_jump_rules(air))
                  : cmd;
    pm.cmd = sh::sequel_command(sent);
    sh::sequel_move(pm, world);
    if (down && pm.ground_entity < 0 && pm.s.velocity[2] > 200 * 8) r.jumps++;
    const sh::Vec3f v = unpack(pm.s.velocity), o = unpack(pm.s.origin);
    if (i > 0 && o.x - last.x < -4096) shift.x += 8192;
    if (i > 0 && o.x - last.x > 4096) shift.x -= 8192;
    if (i > 0 && o.y - last.y < -4096) shift.y += 8192;
    if (i > 0 && o.y - last.y > 4096) shift.y -= 8192;
    last = o;
    r.speed.x.push_back(static_cast<double>(i + 1) / kRate);
    r.speed.y.push_back(plan_speed(v));
    r.path.x.push_back(o.x + shift.x);
    r.path.y.push_back(o.y + shift.y);
    if (turn != 0 && i >= 3 * kRate && r.reached < 0) {
      double off = ge::core::libm::atan2(v.y, v.x) * 180 / kPi - turn;
      while (off < -180) off += 360;
      if (off > -kSpeedJumpNear && off < kSpeedJumpNear)
        r.reached = static_cast<double>(i + 1 - 3 * kRate) / kRate;
    }
  }
  r.end = static_cast<float>(r.speed.y.back());
  return r;
}

void shoot_chart(gl::Lab& lab, const std::string& id, const gl::LineChart& spec,
                 const std::string& caption, const std::string& look_for) {
  const gl::Chart chart = gl::line_chart(spec);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, {}}, gl::to_picture(chart.plot));
}

void move_alone(gl::Lab& lab) {
  struct Law {
    float air;
    const char* key;
    const char* name;
  };
  const Law laws[] = {{0, "air0", "sv_airaccelerate 0"}, {1, "air1", "sv_airaccelerate 1"}};
  std::vector<gl::Trace> speed_steer;
  gl::LabTable table{
      "the sequel's move alone: eight seconds of forward and jump after a second's "
      "run-up",
      {"air", "hands", "jumps", "speed at 3 s", "speed at 9 s", "gain a second"},
      {}};
  for (const Law& law : laws) {
    const MoveRun held = move_run(0, law.air, "forward + jump, cl_speedjump 0");
    const MoveRun half = move_run(0.5f, law.air, "cl_speedjump 1, pm_speedjump 0.5");
    const MoveRun most = move_run(kShare, law.air, "cl_speedjump 1, pm_speedjump 0.8");
    const MoveRun all = move_run(1, law.air, "cl_speedjump 1, pm_speedjump 1");
    for (const MoveRun* r : {&held, &half, &most, &all}) {
      const double at3 = r->speed.y[static_cast<size_t>(3 * kRate - 1)];
      table.rows.push_back({law.name, r->speed.label, std::to_string(r->jumps), to_fixed(at3, 1),
                            to_fixed(r->end, 1), to_fixed((r->end - 300) / kSeconds, 1)});
    }
    const std::string k = std::string("move.") + law.key;
    lab.metric(k + ".held.speed", held.end, "u/s");
    lab.metric(k + ".held.jumps", held.jumps, "count");
    lab.metric(k + ".easy-0.5.speed", half.end, "u/s");
    lab.metric(k + ".easy.speed", most.end, "u/s");
    lab.metric(k + ".easy.jumps", most.jumps, "count");
    lab.metric(k + ".easy-1.speed", all.end, "u/s");
    if (held.jumps != 1)
      lab.fail(std::string(law.name) + ": the jump held without the aid jumped " +
                   std::to_string(held.jumps) + " times",
               "The sequel's jump needs a let-go (sequel_move.cpp's jump). Skill: shooter.");
    if (most.jumps < 10 || most.end <= held.end + 150)
      lab.fail(std::string(law.name) +
                   ": the easy jump gained too little: " + to_fixed(most.end, 1) +
                   " u/s after 8 s, " + std::to_string(most.jumps) + " jumps",
               "Read speedjump_assist.cpp (along, sequel_jump_rules) and the let-go of the up "
               "move. Skill: shooter.");
    if (!(half.end < most.end && most.end < all.end))
      lab.fail(std::string(law.name) + ": the shares do not gain in order",
               "Read speedjump_assist.cpp's along. Skill: shooter.");

    gl::LineChart chart;
    chart.spikes = false;
    chart.frame = {std::string("PLAN SPEED OVER TIME: THE SEQUEL'S MOVE, ") + law.name,
                   "13 ms commands on an open floor; a second's run-up on the ground first",
                   "t (s)", "plan speed (units/s)"};
    chart.traces = {held.speed, half.speed, most.speed, all.speed};
    chart.rules = {300};
    shoot_chart(
        lab, std::string("speed-") + law.key, chart,
        std::string("The player's plan speed command by command on the sequel's move with ") +
            law.name +
            (law.air == 0
                 ? " (its default, and always in single player: the air adds up to the whole "
                   "wish speed at an acceleration of 1, so the aid holds the wish near square to "
                   "the way and turns slowly). "
                 : " (deathmatch: the classic cap of 30 at an acceleration of 10). ") +
            "Forward and jump held without the aid jumps once (the sequel's jump needs a let-go) "
            "and runs at 300 (dashed). With cl_speedjump 1 the client lets the jump go in the air, "
            "presses it on landing and strafes: every hop gains by the share.",
        "three easy lines climbing in order of their share, the plain one flat at 300");
    if (law.air == 0) {
      gl::LineChart top;
      top.spikes = false;
      top.frame = {"THE HOPS FROM ABOVE", "sv_airaccelerate 0, pm_speedjump 0.8, the aim east",
                   "x (units)", "y (units)"};
      gl::Trace aim{"the aim east", {0, most.path.x.back()}, {0, 0}};
      top.traces = {aim, held.path, most.path};
      shoot_chart(lab, "paths", top,
                  "Both runs from above: the plain hands go straight at 300, the easy jump's hops "
                  "weave across the aim line as a strafer's do (to both sides in turn), and go "
                  "further for their speed.",
                  "a straight short line and a weaving long one along y = 0");
      // The steer: the aim turned a quarter, then half a turn, left at 3 s.
      const MoveRun quarter = move_run(kShare, law.air, "aim turned 90 left at 3 s", 90);
      const MoveRun back = move_run(kShare, law.air, "aim turned 180 at 3 s", 180);
      lab.metric("move.air0.steer-90.reached", quarter.reached, "s");
      lab.metric("move.air0.steer-180.reached", back.reached, "s");
      lab.metric("move.air0.steer-90.speed", quarter.end, "u/s");
      lab.metric("move.air0.steer-180.speed", back.end, "u/s");
      if (quarter.reached < 0 || back.reached < 0)
        lab.fail("the easy jump did not turn the hops to a turned aim on the sequel's move",
                 "Read speedjump_assist.cpp's steer on foot (kSpeedJumpSteer). Skill: shooter.");
      gl::LineChart steer;
      steer.spikes = false;
      steer.frame = {"THE STEER FROM ABOVE",
                     "sv_airaccelerate 0, pm_speedjump 0.8: the aim east, turned at 3 s",
                     "x (units)", "y (units)"};
      steer.traces = {quarter.path, back.path};
      shoot_chart(lab, "steer", steer,
                  "The aim turned at 3 s, a quarter left and half a turn. The sequel's air turns "
                  "a hop slowly at speed (its turn a command is the gain's step over the speed), "
                  "so a far-off aim is turned to on foot: the aid lands, holds the jump back "
                  "while the walk turns him (losing speed as a player does), then hops on. "
                  "Reached in " +
                      to_fixed(quarter.reached, 2) + " s and " + to_fixed(back.reached, 2) + " s.",
                  "both paths bending onto their new aims, then weaving on along them");
      speed_steer = {quarter.speed, back.speed};
    }
  }
  lab.table(table);
  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"PLAN SPEED OVER TIME: THE STEER", "sv_airaccelerate 0, pm_speedjump 0.8", "t (s)",
                 "plan speed (units/s)"};
  chart.traces = speed_steer;
  chart.rules = {300};
  shoot_chart(lab, "speed-steer", chart,
              "The two steering runs' speed: each turn of the aim costs the speed the walk's "
              "friction takes while it turns him on foot, and the hops gain it back.",
              "a climb, a drop when the aim turns, a climb again");
}

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

// THE HALL: a walled floor 8000 square of brushes (a run turning left at the default 30 degrees a
// second circles inside it), a deathmatch spot and a start east of its middle facing north (+y).
std::unique_ptr<cl::LoadedMap> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  constexpr float kHalf = 4000;
  m.push_back(box(-kHalf, -kHalf, -16, kHalf, kHalf, 0));
  m.push_back(box(-kHalf - 16, -kHalf, -16, -kHalf, kHalf, 256));
  m.push_back(box(kHalf, -kHalf, -16, kHalf + 16, kHalf, 256));
  m.push_back(box(-kHalf - 16, kHalf, -16, kHalf + 16, kHalf + 16, 256));
  m.push_back(box(-kHalf - 16, -kHalf - 16, -16, kHalf + 16, -kHalf, 256));
  for (const char* kind : {"info_player_deathmatch", "info_player_start"}) {
    br::Entity e;
    e.pairs = {{"classname", kind}, {"origin", "1600 -1200 24"}, {"angle", "90"}};
    w.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  if (!br::write_brush_world(w, &bytes, &error)) return nullptr;
  return cl::map_from_bytes(bytes, "sjhall", error);
}

struct GameRun {
  gl::Trace speed, path;
  float top = 0;
};

// On a real map: the player moved to the longest open run the map has, so his hops are not
// stopped by the first wall. From his spawn and every item, 16 headings are tried: the run is the
// shortest clear sweep of his box along three lines 64 units apart, cut where the floor falls away
// by more than 96 units.
void to_open_run(cl::Listen& l, sv::SequelServer& sq) {
  sh::SequelBrushWorld bw{sq.tracer.get(), sh::kSqMaskPlayerSolid};
  const sh::SequelWorld world = bw.world();
  const sh::Vec3f mins{-16, -16, -24}, maxs{16, 16, 32};
  const auto trace = [&](sh::Vec3f a, sh::Vec3f b) {
    return world.trace(world.ctx, a, mins, maxs, b);
  };
  const auto run_length = [&](sh::Vec3f from, double yaw) {
    const double r = yaw * kPi / 180;
    const auto cx = static_cast<float>(ge::core::libm::cos(r));
    const auto cy = static_cast<float>(ge::core::libm::sin(r));
    float most = 3000;
    for (const float side : {-32.0f, 0.0f, 32.0f}) {
      const sh::Vec3f a{from.x - cy * side, from.y + cx * side, from.z};
      if (trace(a, a).start_solid) return 0.0f;
      most = std::min(most, trace(a, {a.x + cx * 3000, a.y + cy * 3000, a.z}).fraction * 3000);
    }
    for (int step = 1; static_cast<float>(step) * 128 < most; step++) {
      const float d = static_cast<float>(step) * 128;
      const sh::Vec3f p{from.x + cx * d, from.y + cy * d, from.z};
      if (trace(p, {p.x, p.y, p.z - 96}).fraction == 1) return d;
    }
    return most;
  };
  std::vector<sh::Vec3f> starts = {unpack(sq.players[0].move.origin)};
  for (const sv::SequelEntity& e : sq.entities)
    if (e.item >= 0 && !e.gone) starts.push_back(e.origin);
  float best = -1, best_yaw = 0;
  sh::Vec3f best_at = starts[0];
  for (size_t i = 0; i < starts.size(); i++) {
    sh::Vec3f from = starts[i];
    if (i > 0) {  // stood on the floor under the item
      const sh::SequelTrace down = trace({from.x, from.y, from.z + 32}, {from.x, from.y, -4096});
      if (down.start_solid || down.fraction == 1) continue;
      from = down.end;
      from.z += 1;
    }
    for (int k = 0; k < 16; k++) {
      const float len = run_length(from, k * 22.5);
      if (len > best) best = len, best_yaw = static_cast<float>(k * 22.5), best_at = from;
    }
  }
  sv::SequelPlayer& me = sq.players[0];
  for (int k = 0; k < 3; k++) {
    const float c = k == 0 ? best_at.x : k == 1 ? best_at.y : best_at.z;
    me.move.origin[k] = static_cast<int16_t>(ge::core::num::to_int32(double(c) * 8));
    me.move.velocity[k] = 0;
  }
  l.view = {0, best_yaw, 0};
}

// The player on `map` with `cl_speedjump` as asked and `view` of the swing shown: two seconds to
// spawn and settle, a second of forward alone, then forward and jump held, the view turning left
// at `turn` degrees a second (from the map's longest open run when `open_run`). `at` is called
// mid-swing from `at_s` seconds into the hopping.
template <class At>
bool game_run(std::unique_ptr<cl::LoadedMap> map, bool open_run, bool speedjump, float view,
              float turn, double at_s, GameRun& out, std::string& error, At at) {
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  cl::Listen l;
  if (!map ||
      !cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error))
    return false;
  sv::SequelServer* sq = sv::sequel_of(*l.rules);
  if (sq == nullptr) {
    error = "the map did not play on the sequel's rules";
    return false;
  }
  l.rules->user_info(0, "cl_speedjump", speedjump ? "1" : "0");
  l.speedjump_view = view;
  for (int i = 0; i < 2 * kRate; i++) cl::listen_frame(l, {});
  if (open_run) {
    to_open_run(l, *sq);
    for (int i = 0; i < kRate / 2; i++) cl::listen_frame(l, {});
  }
  out.speed.label = speedjump ? "cl_speedjump 1" : "cl_speedjump 0";
  out.path.label = out.speed.label;
  sh::MoveCmd cmd;
  cmd.forward = 400;
  const int at_cmd = kRate + static_cast<int>(at_s * kRate);
  bool done = false;
  for (int i = 0; i < (1 + kSeconds) * kRate; i++) {
    if (i == kRate) cmd.buttons = sh::kButtonJump;  // a second's run-up, then the jump held
    l.view.y = l.view.y + turn / kRate;
    cl::listen_frame(l, cmd);
    const sv::SequelPlayer& me = sq->players[0];
    const sh::Vec3f v = unpack(me.move.velocity), o = unpack(me.move.origin);
    out.top = std::max(out.top, plan_speed(v));
    out.speed.x.push_back(static_cast<double>(i + 1) / kRate);
    out.speed.y.push_back(plan_speed(v));
    out.path.x.push_back(o.x);
    out.path.y.push_back(o.y);
    if (!done && i + 1 >= at_cmd && std::abs(l.jump_aid.swing) > 15) {
      done = true;
      at(l, *sq);
    }
  }
  cl::listen_stop(l);
  return true;
}

void in_game(gl::Lab& lab, uint32_t w, uint32_t h) {
  std::string error;
  const float turn = static_cast<float>(lab.arg_num("turn"));
  GameRun off, on;
  const auto none = [](cl::Listen&, sv::SequelServer&) {};
  if (!game_run(hall(error), false, false, 0, turn, 0, off, error, none) ||
      !game_run(hall(error), false, true, 0.5f, turn, 0, on, error, none)) {
    lab.fail(error, "The hall should play on the sequel's rules. Skill: shooter.");
    return;
  }
  lab.metric("game.off.top-speed", off.top, "u/s");
  lab.metric("game.on.top-speed", on.top, "u/s");
  if (on.top <= off.top + 100)
    lab.fail("in the game the easy jump gained too little: " + to_fixed(on.top, 1) + " u/s",
             "Check the user info reached the slot (server/sequel_server.cpp sq_user_info), the "
             "jump button reached the move as the up move (sequel_command) and the aid read the "
             "sequel's player (client/listen.cpp). Skill: shooter.");

  gl::LineChart speed;
  speed.spikes = false;
  speed.frame = {"PLAN SPEED OVER TIME: IN THE GAME",
                 "the window's own server on a hall of brushes (the sequel's rules): a second's "
                 "run, then forward and jump held; the view turning " +
                     to_fixed(turn, 0) + " degrees a second",
                 "t (s)", "plan speed (units/s)"};
  speed.traces = {off.speed, on.speed};
  speed.rules = {300};
  shoot_chart(lab, "speed-game", speed,
              "The server's own player each frame, the same hands twice: with cl_speedjump 0 he "
              "jumps once and runs at 300 (dashed); with cl_speedjump 1 the client strafes for "
              "him at the window's pm_speedjump 0.8 and every hop adds speed while the hops "
              "follow the turning view.",
              "the cl_speedjump 1 line climbing in a saw-tooth of hops, the other flat at 300");
  gl::LineChart top;
  top.spikes = false;
  top.frame = {"THE PATHS FROM ABOVE", "the hall, its walls at +-4000", "x (units)", "y (units)"};
  gl::Trace walls{"walls", {-4000, 4000, 4000, -4000, -4000}, {-4000, -4000, 4000, 4000, -4000}};
  top.traces = {walls, off.path, on.path};
  shoot_chart(lab, "paths-game", top,
              "Both runs from above from the same spawn: the easy run weaves its hops along the "
              "way the view turns, and goes further for its speed.",
              "two paths from the same spawn, the cl_speedjump 1 one weaving and longer");

  // The eyes, on a real map of the sequel's (private pictures only).
  if (lab.arg("game").empty()) return;
  const pf::GameProfile& game = *pf::find_game("sequel");
  const cl::GameAdapter& adapter = cl::adapter_of(game);
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                        "/pak0.pak>. Skill: shooter.");
    return;
  }
  const std::string name = lab.arg("map");
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes =
      cl::find_file(files, ge::classicenhance::asset_path(content.maps, name), store);
  const std::vector<uint8_t> file(bytes.begin(), bytes.end());
  auto device = ge::gpu::create_device({.label = "se-sequel-speedjump", .validation = 1});
  if (!device) {
    lab.note("no GPU device: the eyes are not drawn");
    return;
  }
  ge::gpu::Target target = ge::gpu::create_target(*device, "se-sequel-speedjump.final", w, h);
  shooters_edge::look::Picture look;
  look.look = lab.arg("look");
  const double at_s = lab.arg_num("at");
  float speed_at = 0;
  const auto eyes = [&](float view) {
    return [&, view](cl::Listen& l, sv::SequelServer& sq) {
      const sv::SequelPlayer& me = sq.players[0];
      speed_at = plan_speed(unpack(me.move.velocity));
      std::string err;
      cl::WorldDrawPtr world =
          cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look,
                                {}, err, {}, adapter);
      if (!world) {
        lab.fail(err, "Read the gpu: lines above. Skill: render.");
        return;
      }
      const sh::Vec3f o = unpack(me.move.origin);
      ge::render::FrameView frame;
      const ge::camera::Vec3 eye = cl::engine_point({o.x, o.y, o.z + me.view_height});
      frame.eye[0] = eye.x;
      frame.eye[1] = eye.y;
      frame.eye[2] = eye.z;
      frame.heading = cl::engine_heading(l.view.y + l.jump_aid.shown);
      frame.fov_y = 1.2;
      frame.time_s = 1;
      const cl::FxFrame fx;
      cl::draw_world(*device, *world, frame, fx, nullptr, 0, {}, target.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, target);
      const std::string id = view > 0 ? "eyes-swing" : "eyes-still";
      lab.shot(
          {id,
           view > 0 ? "MID-HOP ON " + name + ", CL_SPEEDJUMP_VIEW 1: THE SWING SHOWN"
                    : "MID-HOP ON " + name + ", CL_SPEEDJUMP_VIEW 0: A STILL VIEW",
           "From the player's eyes on " + name + ", " + to_fixed(at_s, 1) +
               " s into holding forward and jump with cl_speedjump 1 on the sequel's rules, "
               "at " +
               to_fixed(speed_at, 0) + " units/s (the run speed is 300). " +
               (view > 0 ? "cl_speedjump_view 1: the view turned " + to_fixed(l.jump_aid.shown, 1) +
                               " degrees off his aim, where a strafer would look."
                         : "cl_speedjump_view 0: the view on his own aim while the client "
                           "swings the sent yaw."),
           "the same moment as the other eyes picture, the view turned by the swing or not",
           {{"map", name}, {"game", "local packs"}, {"look", look.look}}},
          gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
          gl::Det::measured);
    };
  };
  GameRun swing_run, still;
  if (!game_run(cl::map_from_bytes(file, name, error), true, true, 1, 0, at_s, swing_run, error,
                eyes(1)) ||
      !game_run(cl::map_from_bytes(file, name, error), true, true, 0, 0, at_s, still, error,
                eyes(0))) {
    lab.fail(name + ": " + error, "Pass --map=<a map in --game's packs>. Skill: shooter.");
    return;
  }
  lab.metric("eyes.speed-at", speed_at, "u/s", gl::Det::measured);
  lab.metric("eyes.top-speed", swing_run.top, "u/s", gl::Det::measured);
  if (swing_run.speed.y != still.speed.y)
    lab.fail("the shown swing changed the move",
             "cl_speedjump_view only turns the drawn view (Listen::jump_aid.shown). Skill: "
             "shooter.");
  gl::LineChart real;
  real.spikes = false;
  real.frame = {"PLAN SPEED OVER TIME ON " + name,
                "from the map's longest open run: a second's run, then forward and jump held",
                "t (s)", "plan speed (units/s)"};
  swing_run.speed.label = "cl_speedjump 1 on " + name;
  real.traces = {swing_run.speed};
  real.rules = {300};
  shoot_chart(lab, "speed-" + name, real,
              "The easy run the eyes pictures were drawn from, on the map's own brushes: the hops "
              "gain until a wall or a drop stops them.",
              "a climbing saw-tooth from 300 until the run ends");
}

void run(gl::Lab& lab) {
  move_alone(lab);
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  in_game(lab, w, h);
}

const gl::LabSpec kSpec{
    "se-sequel-speedjump",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the easy jump (cl_speedjump) on the sequel's move: its air laws, its up-move jump, its wiring",
    "shooter",
    "Does holding forward and jump with cl_speedjump 1 hop on and gain speed on the sequel's move, "
    "under both of its air laws and on the window's own sequel server?",
    {{"size", "960,540", "the eyes' pictures' width and height, px"},
     {"game", "",
      "the folder holding the sequel's base folder of packs (private pictures only); none: no "
      "eyes"},
     {"map", "q2dm1", "the map in --game's packs the eyes are drawn on"},
     {"look", "modern", "the look the eyes are drawn in (classic or modern)"},
     {"turn", "30", "degrees a second the view turns in the hall"},
     {"at", "4", "seconds into the hopping the eyes are drawn"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
