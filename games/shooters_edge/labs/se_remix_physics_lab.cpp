// LAB `se-remix-physics`: THE PLAYERS' BODIES UNDER CLASSIC REMIX'S `cr_physics`. Each game's move
// keeps its own code; `cr_physics` sets the other game's physics on it
// (profiles/remix_arsenal.hpp's `kRemixPhysics`): the running speed, friction, the air, the landing
// and the water. On a flat floor of brushes a scripted player runs forward for a second, then
// strafe-jumps weaving at each move's best angle for four; in a pool he swims forward for two. Each
// under four rules: the classic move as itself and told the sequel's numbers, and the sequel's move
// as itself and given the classic game's (`sv_physics classic`). The harm a fall does is the rules'
// (the classic arena's `sv_falldamage`), held by tests/shooters_edge_remix_test.cpp, not here.
//
// What it shows: a chart of each run's speed over time, and a table of the speed it runs at, the
// speed after four seconds of strafe-jumping, the most it reached and the swimming speed. Read it
// for the feel: the classic game's air gains speed hop after hop, the sequel's barely does, and
// the classic swimmer is half again as fast; each move told the other game's numbers should sit by
// the other game's own line. Deterministic: the digest holds the speeds.
//
// ge:summary Lab: the classic and sequel moves under cr_physics, run and strafe-jumped on a floor
// ge:keywords lab se-remix-physics cr_physics classic-remix movement-feel strafe-jump bunny-hop
// ge:keywords sv_physics pm_airaccel pm_airwish pm_swimscale run-speed air-gain swim-speed
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/pmove.hpp"
#include "shooter/sequel_move.hpp"
#include "shooter/settings.hpp"
#include "shooter/world.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace lm = ge::core::libm;
namespace pr = shooters_edge::profiles;

constexpr int kMsec = 10;        // a command every 10 ms
constexpr double kRunS = 1;      // running forward
constexpr double kHopS = 4;      // then strafe-jumping
constexpr double kSwimS = 2;     // swimming forward, in the pool
constexpr float kPoolTop = 512;  // the pool's water fills the room to here
constexpr double kPi = 3.14159265358979323846;
// The floor's half width, inside the sequel's packed origins (eighths in 16 bits), and where the
// run starts: in the middle, as the weave drifts off its line by a few degrees a hop.
constexpr float kFloorHalf = 4088;
constexpr float kStartX = 0;

// The floor, written and read back as a map of brushes.
struct Floor {
  br::Map map;
  std::unique_ptr<br::BrushTracer> tracer;
  bool ok = false;
  std::string error;

  explicit Floor(bool pool) {
    br::BrushWorld w;
    w.models.resize(1);
    const float lo[3] = {-kFloorHalf, -kFloorHalf, -16}, hi[3] = {kFloorHalf, kFloorHalf, 0};
    w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
    // A ceiling far above, so the air between is inside the map (outside its bounds is solid).
    const float top_lo[3] = {-kFloorHalf, -kFloorHalf, 1024},
                top_hi[3] = {kFloorHalf, kFloorHalf, 1040};
    w.models[0].push_back(br::box_brush(top_lo, top_hi, sh::kSqSolid, "world/floor"));
    const float water_lo[3] = {-kFloorHalf, -kFloorHalf, 0},
                water_hi[3] = {kFloorHalf, kFloorHalf, kPoolTop};
    if (pool) w.models[0].push_back(br::box_brush(water_lo, water_hi, sh::kSqWater, "world/water"));
    std::vector<uint8_t> bytes;
    ok = br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, &map, &error);
    if (ok) tracer = std::make_unique<br::BrushTracer>(map);
  }
};

// The steer at a moment: the yaw (degrees), the side key's sign (0: forward) and the jump.
struct Steer {
  double yaw = 0;
  int side = 0;
  bool jump = false;
};

// The best angle between the velocity and the wish for the most speed after one command: the
// gain is measured against `cap` (the wish itself when 0), the amount `accel` x the wish x dt.
double best_angle(double speed, double wish, double cap, double accel) {
  const double c = cap > 0 && cap < wish ? cap : wish;
  const double a = accel * wish * kMsec * 0.001;
  if (speed <= 0 || speed <= c - a) return 0;
  const double cosine = std::clamp((c - a) / speed, -1.0, 1.0);
  return lm::acos(cosine) * 180 / kPi;
}

// The script: forward along +x for kRunS, then the side key alone, the view turned so the wish
// sits at the best angle off the velocity, jumping on every landing and changing sides with each
// jump (the weave), so the path keeps near its line. `side` carries the sign between commands.
Steer steer(bool swim, double t, double vx, double vy, bool on_ground, bool jumped_last,
            double wish, double cap, double accel, int& side) {
  Steer s;
  if (t < kRunS || swim) return s;
  s.jump = on_ground && !jumped_last;
  if (side == 0 || s.jump) side = side > 0 ? -1 : 1;
  s.side = side;
  const double heading = lm::atan2(vy, vx) * 180 / kPi;
  const double speed = std::sqrt(vx * vx + vy * vy);
  // The side key's wish lies at the view's yaw less 90 x side; it wants the heading less side x
  // the best angle.
  s.yaw = heading - side * best_angle(speed, wish, cap, accel) + 90 * side;
  return s;
}

struct Run {
  std::string label;
  std::vector<double> t, speed;
  double run_speed = 0, final_speed = 0, top = 0, swim_speed = 0;
};

void sample(Run& r, bool swim, double t, double vx, double vy) {
  const double v = std::sqrt(vx * vx + vy * vy);
  if (swim) {
    r.swim_speed = v;
    return;
  }
  r.t.push_back(t);
  r.speed.push_back(v);
  if (t < kRunS) r.run_speed = v;
  r.final_speed = v;
  r.top = std::max(r.top, v);
}

// The classic move with `lines` set on its server settings (the sequel's numbers, or none): on
// the floor (`r`'s run), or in the pool (`swim`: its swimming speed).
void classic_run(Floor& f, Run& r, bool swim, std::span<const std::string_view> lines,
                 std::string& error) {
  sh::ServerSettings set;
  for (const std::string_view line : lines) {
    const size_t sp = line.find(' ');
    if (line.starts_with("set ")) continue;  // the rules' own (sv_falldamage)
    if (!sh::set_setting(set, line.substr(0, sp), line.substr(sp + 1), error)) return;
  }
  sh::MoveWorld world;
  world.solids[0] = sh::brushes_solid(*f.tracer, 0, {0, 0, 0}, {-kFloorHalf, -kFloorHalf, -16},
                                      {kFloorHalf, kFloorHalf, 1040});
  world.count = 1;
  sh::MoveState s;
  s.origin = {kStartX, 0, swim ? kPoolTop / 2 : 24.5f};
  s.maxspeed = set.move.maxspeed;
  const double wish = std::min(400.0, double(set.move.maxspeed));
  const double accel = set.move.air_accel != 0 ? set.move.air_accel : set.move.accelerate;
  bool jumped = false;
  int side = 0;
  for (int i = 0; i * kMsec * 0.001 < (swim ? kSwimS : kRunS + kHopS); ++i) {
    const double t = i * kMsec * 0.001;
    const Steer st = steer(swim, t, s.velocity.x, s.velocity.y, s.ground >= 0, jumped, wish,
                           set.move.air_wish_cap, accel, side);
    sh::MoveCmd c;
    c.msec = kMsec;
    c.angles = {0, float(st.yaw), 0};
    c.forward = st.side == 0 ? 400 : 0;
    c.side = int16_t(400 * st.side);
    c.buttons = st.jump ? sh::kButtonJump : 0;
    jumped = st.jump;
    sh::player_move(s, c, set.move, world);
    sample(r, swim, t, s.velocity.x, s.velocity.y);
  }
}

// The sequel's move with `feel` on it (its own, or the classic game's), on the floor or in the
// pool (`swim`).
void sequel_run(Floor& f, Run& r, bool swim, const pr::MoveFeel& feel) {
  sh::SequelBrushWorld brushes;
  brushes.tracer = f.tracer.get();
  sh::SequelMove pm;
  pm.s.origin[0] = int16_t(kStartX * 8);
  pm.s.origin[2] = int16_t((swim ? kPoolTop / 2 : 24.5f) * 8);
  pm.max_speed = feel.max_speed;
  pm.friction = feel.friction;
  pm.air_accelerate = feel.air_accelerate;
  pm.land_pause = feel.land_pause;
  pm.swim_scale = feel.swim_scale;
  pm.water_friction = feel.water_friction;
  pm.water_jump = feel.water_jump;
  pm.changed = true;
  const double wish = std::min(400.0, double(feel.max_speed));
  // The sequel's air: the strafing rule at 10 with a set air acceleration, else 1 by the whole
  // wish.
  const double cap = feel.air_accelerate != 0 ? 30 : 0;
  const double accel = feel.air_accelerate != 0 ? 10 : 1;
  bool jumped = false;
  int side = 0;
  for (int i = 0; i * kMsec * 0.001 < (swim ? kSwimS : kRunS + kHopS); ++i) {
    const double t = i * kMsec * 0.001;
    const double vx = pm.s.velocity[0] * 0.125, vy = pm.s.velocity[1] * 0.125;
    const Steer st = steer(swim, t, vx, vy, pm.ground_entity >= 0, jumped, wish, cap, accel, side);
    sh::SequelCmd c;
    c.msec = kMsec;
    c.angles[1] = int16_t(int(std::floor(st.yaw * 65536 / 360 + 0.5)) & 0xffff);
    c.forward = st.side == 0 ? 400 : 0;
    c.side = int16_t(400 * st.side);
    c.up = st.jump ? sh::kSqJumpUp : 0;
    jumped = st.jump;
    pm.cmd = c;
    sh::sequel_move(pm, brushes.world());
    pm.changed = false;
    sample(r, swim, t, pm.s.velocity[0] * 0.125, pm.s.velocity[1] * 0.125);
  }
}

void run(gl::Lab& lab) {
  Floor floor(false), pool(true);
  if (!floor.ok || !pool.ok) {
    lab.fail("se-remix-physics: the floor or the pool did not build: " + floor.error + pool.error,
             "Read the brush writer's reason above. Skill: brush.");
    return;
  }
  std::string error;
  const pr::RemixPhysics* as_sequel = pr::remix_physics("classic", "sequel");
  std::vector<Run> runs(4);
  runs[0].label = "classic";
  runs[1].label = "classic, cr_physics 2";
  runs[2].label = "sequel";
  runs[3].label = "sequel, cr_physics 1";
  for (const bool swim : {false, true}) {
    classic_run(swim ? pool : floor, runs[0], swim, {}, error);
    classic_run(swim ? pool : floor, runs[1], swim, as_sequel->lines, error);
    sequel_run(swim ? pool : floor, runs[2], swim, pr::MoveFeel{});
    sequel_run(swim ? pool : floor, runs[3], swim, pr::kClassicFeel);
  }
  if (!error.empty()) {
    lab.fail(
        "se-remix-physics: a line of cr_physics 2 was refused: " + error,
        "Fix the line in profiles/remix_arsenal.hpp's kClassicPhysicsAsSequel. Skill: shooter.");
    return;
  }

  gl::LabTable table{
      "THE BODIES UNDER CR_PHYSICS",
      {"rules", "running (u/s)", "after 4 s of hops (u/s)", "most (u/s)", "swimming (u/s)"},
      {}};
  for (const Run& r : runs) {
    const std::string key = r.label.substr(0, r.label.find(',')) +
                            (r.label.find(',') == std::string::npos ? "" : ".remixed");
    lab.metric(key + ".run", r.run_speed, "u/s");
    lab.metric(key + ".hops", r.final_speed, "u/s");
    lab.metric(key + ".swim", r.swim_speed, "u/s");
    table.rows.push_back({r.label, ge::core::num::to_fixed(r.run_speed, 0),
                          ge::core::num::to_fixed(r.final_speed, 0),
                          ge::core::num::to_fixed(r.top, 0),
                          ge::core::num::to_fixed(r.swim_speed, 0)});
  }
  lab.table(table);

  if (lab.wants("speed")) {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"SPEED UNDER CR_PHYSICS",
               "a second's run, then four of strafe-jumping",
               "t (s)",
               "speed (u/s)",
               960,
               420};
    for (const Run& r : runs) c.traces.push_back({r.label, r.t, r.speed});
    c.rules = {300, 320};
    const gl::Chart ch = gl::line_chart(c);
    lab.shot({"speed",
              ch.title,
              "Each move's speed on a flat floor: forward for a second, then the side key alone "
              "with the view at the move's best angle, jumping on every landing. The dashed lines "
              "are the sequel's run (300) and the classic game's (320).",
              "each remixed line runs at the other game's speed and gains like it in the air",
              {{"msec", std::to_string(kMsec)}}},
             gl::to_picture(ch.plot));
  }
}

const gl::LabSpec kSpec{
    "se-remix-physics",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/pmove.hpp and sequel_move.hpp under classic remix's cr_physics "
    "(games/shooters_edge/profiles/remix_arsenal.hpp)",
    "shooter",
    "Does each game's move, told the other game's physics by cr_physics, run, strafe-jump and swim "
    "like the other game?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
