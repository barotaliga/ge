// LAB `se-remix-physics`: THE PLAYERS' BODIES UNDER CLASSIC REMIX'S `cr_physics`. Each game's move
// keeps its own code; `cr_physics` sets the other game's physics on it
// (profiles/remix_arsenal.hpp's `kRemixPhysics`): the running speed, friction, the air, the landing
// and the water. On a flat floor of brushes a scripted player runs forward for a second, then
// strafe-jumps weaving at each move's best angle for four; in a pool he swims forward for two. Each
// under four rules: the classic move as itself and told Quake II's numbers, and Quake II's move
// as itself and given Quake's (`sv_physics classic`). The harm a fall does is the rules'
// (the classic arena's `sv_falldamage`), held by tests/shooters_edge_remix_test.cpp, not here.
// THE STAIR JUMP: the player runs at a ledge 24 or 32 high, jumps short of it and holds the jump,
// so a second jump goes off on the first step on the ledge; the table gives each rule's best
// height over the ledge, and a chart draws the best double jump onto the 32 ledge.
//
// What it shows: a chart of each run's speed over time, and a table of the speed it runs at, the
// speed after four seconds of strafe-jumping, the most it reached and the swimming speed. Read it
// for the feel: Quake's air gains speed hop after hop, Quake II's barely does, and
// the classic swimmer is half again as fast; each move told the other game's numbers should sit by
// the other game's own line. Deterministic: the digest holds the speeds.
//
// ge:summary Lab: the classic and Quake II moves under cr_physics, run and strafe-jumped on a floor
// ge:keywords lab se-remix-physics cr_physics classic-remix movement-feel strafe-jump bunny-hop
// ge:keywords sv_physics pm_airaccel pm_airwish pm_swimscale run-speed air-gain swim-speed
// ge:keywords stair-jump double-jump pm_airclimb air-step
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
#include "classic/q2_move.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/pmove.hpp"
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
// The floor's half width, inside Quake II's packed origins (eighths in 16 bits), and where the
// run starts: in the middle, as the weave drifts off its line by a few degrees a hop.
constexpr float kFloorHalf = 4088;
constexpr float kStartX = 0;

// The floor, written and read back as a map of brushes.
struct Floor {
  br::Map map;
  std::unique_ptr<br::BrushTracer> tracer;
  bool ok = false;
  std::string error;

  // `ledge`: a step that high fills the floor's +x half (its edge at x 0), for the stair jump.
  explicit Floor(bool pool, float ledge = 0) {
    br::BrushWorld w;
    w.models.resize(1);
    const float lo[3] = {-kFloorHalf, -kFloorHalf, -16}, hi[3] = {kFloorHalf, kFloorHalf, 0};
    w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
    const float step_lo[3] = {0, -kFloorHalf, 0}, step_hi[3] = {kFloorHalf, kFloorHalf, ledge};
    if (ledge > 0)
      w.models[0].push_back(br::box_brush(step_lo, step_hi, sh::kSqSolid, "world/floor"));
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

// The classic move with `lines` set on its server settings (Quake II's numbers, or none): on
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

// Quake II's move with `feel` on it (its own, or Quake's), on the floor or in the
// pool (`swim`).
void q2_run(Floor& f, Run& r, bool swim, const pr::MoveFeel& feel) {
  ge::classic::Q2BrushWorld brushes;
  brushes.tracer = f.tracer.get();
  ge::classic::Q2Move pm;
  pm.s.origin[0] = int16_t(kStartX * 8);
  pm.s.origin[2] = int16_t((swim ? kPoolTop / 2 : 24.5f) * 8);
  pm.max_speed = feel.max_speed;
  pm.friction = feel.friction;
  pm.air_accelerate = feel.air_accelerate;
  pm.land_pause = feel.land_pause;
  pm.swim_scale = feel.swim_scale;
  pm.water_friction = feel.water_friction;
  pm.water_jump = feel.water_jump;
  pm.air_step = feel.air_step;
  pm.changed = true;
  const double wish = std::min(400.0, double(feel.max_speed));
  // Quake II's air: the strafing rule at 10 with a set air acceleration, else 1 by the whole
  // wish.
  const double cap = feel.air_accelerate != 0 ? 30 : 0;
  const double accel = feel.air_accelerate != 0 ? 10 : 1;
  bool jumped = false;
  int side = 0;
  for (int i = 0; i * kMsec * 0.001 < (swim ? kSwimS : kRunS + kHopS); ++i) {
    const double t = i * kMsec * 0.001;
    const double vx = pm.s.velocity[0] * 0.125, vy = pm.s.velocity[1] * 0.125;
    const Steer st = steer(swim, t, vx, vy, pm.ground_entity >= 0, jumped, wish, cap, accel, side);
    ge::classic::Q2Cmd c;
    c.msec = kMsec;
    c.angles[1] = int16_t(int(std::floor(st.yaw * 65536 / 360 + 0.5)) & 0xffff);
    c.forward = st.side == 0 ? 400 : 0;
    c.side = int16_t(400 * st.side);
    c.up = st.jump ? ge::classic::kSqJumpUp : 0;
    jumped = st.jump;
    pm.cmd = c;
    ge::classic::q2_move(pm, brushes.world());
    pm.changed = false;
    sample(r, swim, t, pm.s.velocity[0] * 0.125, pm.s.velocity[1] * 0.125);
  }
}

// THE STAIR JUMP: running at +x toward a ledge `ledge` high, the player jumps `before` units short
// of its edge, lets go a step, then holds the jump, so a second jump goes off on the first step
// he stands on the ledge. Returns how high his feet rose over the ledge (the higher of the two
// jumps' tops), so a move that adds the jump to the rise it still carries (a double jump) shows
// it. `twice` false: the one jump alone.
constexpr float kRunUp = 400;     // the run starts this far short of the edge
constexpr int kStairSteps = 240;  // the run-up and 0.9 s after the jump at the least

float classic_stair(Floor& f, std::span<const std::string_view> lines, float ledge, float before,
                    bool twice, Run* trace = nullptr) {
  sh::ServerSettings set;
  std::string error;
  for (const std::string_view line : lines) {
    const size_t sp = line.find(' ');
    if (line.starts_with("set ")) continue;
    if (!sh::set_setting(set, line.substr(0, sp), line.substr(sp + 1), error)) return 0;
  }
  sh::MoveWorld world;
  world.solids[0] = sh::brushes_solid(*f.tracer, 0, {0, 0, 0}, {-kFloorHalf, -kFloorHalf, -16},
                                      {kFloorHalf, kFloorHalf, 1040});
  world.count = 1;
  sh::MoveState s;
  s.origin = {-kRunUp, 0, 24.5f};
  s.maxspeed = set.move.maxspeed;
  int jumped_at = -1;
  float top = 0;
  for (int i = 0; i < kStairSteps; ++i) {
    sh::MoveCmd c;
    c.msec = kMsec;
    c.forward = 400;
    if (jumped_at < 0 && s.ground >= 0 && s.origin.x + 16 >= -before) jumped_at = i;
    const bool held = i == jumped_at || (twice && jumped_at >= 0 && i > jumped_at + 1);
    c.buttons = held ? sh::kButtonJump : 0;
    sh::player_move(s, c, set.move, world);
    if (jumped_at >= 0) top = std::max(top, s.origin.z - 24 - ledge);
    if (trace != nullptr && jumped_at >= 0 && i - jumped_at <= 90) {
      trace->t.push_back((i - jumped_at) * kMsec * 0.001);
      trace->speed.push_back(s.origin.z - 24);
    }
  }
  return top;
}

float q2_stair(Floor& f, const pr::MoveFeel& feel, float ledge, float before, bool twice,
               Run* trace = nullptr) {
  ge::classic::Q2BrushWorld brushes;
  brushes.tracer = f.tracer.get();
  ge::classic::Q2Move pm;
  pm.s.origin[0] = int16_t(-kRunUp * 8);
  pm.s.origin[2] = int16_t(24.5f * 8);
  pm.max_speed = feel.max_speed;
  pm.friction = feel.friction;
  pm.air_accelerate = feel.air_accelerate;
  pm.land_pause = feel.land_pause;
  pm.swim_scale = feel.swim_scale;
  pm.water_friction = feel.water_friction;
  pm.water_jump = feel.water_jump;
  pm.air_step = feel.air_step;
  pm.changed = true;
  int jumped_at = -1;
  float top = 0;
  for (int i = 0; i < kStairSteps; ++i) {
    ge::classic::Q2Cmd c;
    c.msec = kMsec;
    c.forward = 400;
    const float x = pm.s.origin[0] * 0.125f;
    if (jumped_at < 0 && pm.ground_entity >= 0 && x + 16 >= -before) jumped_at = i;
    const bool held = i == jumped_at || (twice && jumped_at >= 0 && i > jumped_at + 1);
    c.up = held ? ge::classic::kSqJumpUp : 0;
    pm.cmd = c;
    ge::classic::q2_move(pm, brushes.world());
    pm.changed = false;
    if (jumped_at >= 0) top = std::max(top, pm.s.origin[2] * 0.125f - 24 - ledge);
    if (trace != nullptr && jumped_at >= 0 && i - jumped_at <= 90) {
      trace->t.push_back((i - jumped_at) * kMsec * 0.001);
      trace->speed.push_back(pm.s.origin[2] * 0.125 - 24);
    }
  }
  return top;
}

void run(gl::Lab& lab) {
  Floor floor(false), pool(true);
  if (!floor.ok || !pool.ok) {
    lab.fail("se-remix-physics: the floor or the pool did not build: " + floor.error + pool.error,
             "Read the brush writer's reason above. Skill: brush.");
    return;
  }
  std::string error;
  const pr::RemixPhysics* as_q2 = pr::remix_physics("q1", "q2");
  std::vector<Run> runs(4);
  runs[0].label = "q1";
  runs[1].label = "classic, cr_physics 2";
  runs[2].label = "q2";
  runs[3].label = "Quake II, cr_physics 1";
  for (const bool swim : {false, true}) {
    classic_run(swim ? pool : floor, runs[0], swim, {}, error);
    classic_run(swim ? pool : floor, runs[1], swim, as_q2->lines, error);
    q2_run(swim ? pool : floor, runs[2], swim, pr::MoveFeel{});
    q2_run(swim ? pool : floor, runs[3], swim, pr::kQ1Feel);
  }
  if (!error.empty()) {
    lab.fail("se-remix-physics: a line of cr_physics 2 was refused: " + error,
             "Fix the line in profiles/remix_arsenal.hpp's kQ1PhysicsAsQ2. Skill: shooter.");
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

  // The stair jump over ledges 24 and 32 high (above a step, so climbed only by jumping): each
  // rule's one jump, and its best second jump off the ledge, over every take-off point from the
  // edge to 160 short of it. The best double jump over the 32 ledge is drawn.
  gl::LabTable stairs{"THE STAIR JUMP (feet over the ledge, units)",
                      {"rules", "one jump, 24 ledge", "double jump, 24 ledge", "one jump, 32 ledge",
                       "double jump, 32 ledge"},
                      {}};
  const std::vector<std::string_view> none;
  std::vector<Run> climbs(runs.size());
  const auto stair = [&](size_t k, Floor& step, float ledge, float before, bool twice, Run* tr) {
    return k == 0   ? classic_stair(step, none, ledge, before, twice, tr)
           : k == 1 ? classic_stair(step, as_q2->lines, ledge, before, twice, tr)
           : k == 2 ? q2_stair(step, pr::MoveFeel{}, ledge, before, twice, tr)
                    : q2_stair(step, pr::kQ1Feel, ledge, before, twice, tr);
  };
  for (size_t k = 0; k < runs.size(); ++k) {
    std::vector<std::string> row{runs[k].label};
    for (const float ledge : {24.0f, 32.0f}) {
      Floor step(false, ledge);
      for (const bool twice : {false, true}) {
        float best = 0, best_before = 0;
        for (int n = 0; n <= 80; ++n) {
          const float before = static_cast<float>(2 * n);
          const float top = stair(k, step, ledge, before, twice, nullptr);
          if (top > best) {
            best = top;
            best_before = before;
          }
        }
        if (twice && ledge == 32) {
          climbs[k].label = runs[k].label;
          stair(k, step, ledge, best_before, twice, &climbs[k]);
        }
        const std::string key = runs[k].label.substr(0, runs[k].label.find(',')) +
                                (k % 2 == 1 ? ".remixed" : "") + ".stair" +
                                std::to_string(int(ledge)) + (twice ? ".double" : ".single");
        lab.metric(key, best, "u");
        row.push_back(ge::core::num::to_fixed(best, 1));
      }
    }
    stairs.rows.push_back(row);
  }
  lab.table(stairs);
  if (lab.wants("stair")) {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"THE STAIR JUMP UNDER CR_PHYSICS",
               "the best double jump onto a 32 ledge and off it",
               "t after the first jump (s)",
               "feet over the floor (units)",
               960,
               420};
    for (const Run& r : climbs) c.traces.push_back({r.label, r.t, r.speed});
    c.rules = {32};
    const gl::Chart ch = gl::line_chart(c);
    lab.shot(
        {"stair",
         ch.title,
         "Running at a ledge 32 high, each rule's best take-off: a jump short of the edge, "
         "then the jump held, so a second one goes off on the first step on the ledge. The "
         "Quake II's move climbs the ledge a step while still rising and keeps the rise, so its "
         "second jump adds to it. The classic move rises past the edge first. The dashed "
         "line is the ledge.",
         "each remixed line climbs like the other game's: cr_physics 2 gives the classic "
         "game Quake II's stair jump, cr_physics 1 takes it from Quake II",
         {{"msec", std::to_string(kMsec)}}},
        gl::to_picture(ch.plot));
  }

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
              "are Quake II's run (300) and Quake's (320).",
              "each remixed line runs at the other game's speed and gains like it in the air",
              {{"msec", std::to_string(kMsec)}}},
             gl::to_picture(ch.plot));
  }
}

const gl::LabSpec kSpec{
    "se-remix-physics",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/pmove.hpp and q2_move.hpp under classic remix's cr_physics "
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
