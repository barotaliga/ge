// The scenario table (each one's world, start and script), and the runner that steps one and
// sums what it did.
//
// ge:summary The movement scenarios' worlds, scripts and table, and the runner with its digest
// ge:keywords move scenarios table worlds scripts runner digest fnv
// ge:skill move labs
// ge:test tests/move_scenarios_test.cpp
#include "move/scenarios.hpp"

#include <cstring>

#include "core/math.hpp"
#include "core/num.hpp"
#include "move/push.hpp"
#include "move/strafe.hpp"

namespace ge::move {

namespace {

// ─── the worlds, in metres times `u` ─────────────────────────────────────────────────────────

Vec3 m(double x, double y, double z, double u) { return {x * u, y * u, z * u}; }

void flat(BoxWorld& w, double u) { add_box(w, m(-100, -1, -100, u), m(300, 0, 300, u)); }

void stairs(BoxWorld& w, double u) {
  flat(w, u);
  add_stairs(w, m(-3, 0, 3, u), 8, 0.25 * u, 0.4 * u, 6 * u);  // 2 m up over 3.2 m
  add_box(w, m(-3, 0, 6.2, u), m(3, 2, 60, u));
}

void ramp_walk(BoxWorld& w, double u) {  // 30 degrees: 6 m along, 3.46 m up
  flat(w, u);
  add_ramp(w, m(-3, 0, 3, u), m(3, 3.4641016151377544, 9, u), Rise::pz);
  add_box(w, m(-3, 0, 9, u), m(3, 3.4641016151377544, 60, u));
}

void ramp_steep(BoxWorld& w, double u) {  // 55 degrees: 3 m along, 4.28 m up
  flat(w, u);
  add_ramp(w, m(-3, 0, 3, u), m(3, 4.284444616557465, 6, u), Rise::pz);
  add_box(w, m(-3, 0, 6, u), m(3, 4.284444616557465, 60, u));
}

void tunnel(BoxWorld& w, double u) {  // a ceiling 1.3 m up from 3 m to 6 m along
  flat(w, u);
  add_box(w, m(-3, 1.3, 3, u), m(3, 3, 6, u));
}

void ledge(BoxWorld& w, double u) {  // the ground ends at 5 m; 3 m below it goes on
  add_box(w, m(-100, -1, -100, u), m(300, 0, 5, u));
  add_box(w, m(-100, -4, -100, u), m(300, -3, 300, u));
}

void gap(BoxWorld& w, double u) {  // the ground ends at 5 m; a platform from 9 m; a pit between
  add_box(w, m(-100, -1, -100, u), m(300, 0, 5, u));
  add_box(w, m(-100, -1, 9, u), m(300, 0, 300, u));
  add_box(w, m(-100, -6, -100, u), m(300, -5, 300, u));
}

void platform(BoxWorld& w, double u) {  // a 4 m platform over a pit 6 m down
  add_box(w, m(-100, -7, -100, u), m(300, -6, 300, u));
  add_box(w, m(-2, -1, -2, u), m(2, 0, 2, u));
}

void lift(BoxWorld& w, double u) {  // a 2 m lift in the floor beside a ledge 3 m up
  flat(w, u);
  add_box(w, m(-3, 0, 1, u), m(3, 3, 60, u));
  add_box(w, m(-1, -1, -1, u), m(1, 0, 1, u));
}

void pad(BoxWorld& w, double u) {  // the floor, and a platform 3 m up from 11 m along
  flat(w, u);
  add_box(w, m(-3, 0, 11, u), m(3, 3, 60, u));
}

void trampoline(BoxWorld& w, double u) {  // a 3 m drop at 2 m onto a trampoline floor
  add_box(w, m(-100, -1, -100, u), m(300, 0, 2, u));
  add_box(w, m(-100, -4, 2, u), m(300, -3, 60, u), Surface{{0, 0, 0}, 0, 0.8});
  add_box(w, m(-100, -4, 60, u), m(300, -3, 300, u));
}

void ladder(BoxWorld& w, double u) {  // a 4 m wall from 3 m along, its face a ladder
  flat(w, u);
  add_box(w, m(-3, 0, 3, u), m(3, 4, 60, u), Surface{{0, 0, 0}, kLadder, 0});
}

void pusher(BoxWorld& w, double u) {  // a wall 3 m ahead that will come toward the body
  flat(w, u);
  add_box(w, m(-3, 0, 3, u), m(3, 3, 4, u));
}

// ─── what the worlds do ──────────────────────────────────────────────────────────────────────

// The last brush moved by `delta`, the body carried with it.
void shift(BoxWorld& w, const World& world, const MoveSpec& spec, MoveState& s, const Vec3& delta,
           double dt) {
  const size_t last = w.brushes.size() - 1;
  move_brush(w, last, delta, dt);
  carry(spec, world, s, static_cast<uint32_t>(last + 1), delta);
}

void slide_platform(BoxWorld& w, const World& world, const MoveSpec& spec, MoveState& s, double dt,
                    ScriptMemory&) {
  shift(w, world, spec, s, {0, 0, 2 * spec.unit * dt}, dt);  // 2 m/s along
}

void raise_lift(BoxWorld& w, const World& world, const MoveSpec& spec, MoveState& s, double dt,
                ScriptMemory&) {
  const double top = w.brushes.back().hi.y, end = 3 * spec.unit;
  double rise = s.t >= 0.5 ? 1 * spec.unit * dt : 0;  // 1 m/s, from 0.5 s, to 3 m
  if (rise > end - top) rise = end - top;
  shift(w, world, spec, s, {0, rise, 0}, dt);
}

void knock(BoxWorld&, const World&, const MoveSpec& spec, MoveState& s, double, ScriptMemory& mem) {
  if (s.t < 0.5 || mem.jumps != 0) return;
  push(s, {0, 9.5 * spec.unit, 12.5 * spec.unit});  // a blast from behind: up and forward
  mem.jumps = 1;
}

void jump_pad(BoxWorld&, const World&, const MoveSpec& spec, MoveState& s, double,
              ScriptMemory& mem) {
  const double z = s.pos.z / spec.unit;
  if (mem.jumps != 0 || !s.grounded || z < 3 || z > 4) return;
  const Vec3 target{s.pos.x, 3 * spec.unit, 13 * spec.unit};
  launch(spec, s, arc_velocity(s.pos, target, 1.5 * spec.unit, spec.gravity));
  mem.jumps = 1;
}

void push_wall(BoxWorld& w, const World& world, const MoveSpec& spec, MoveState& s, double dt,
               ScriptMemory&) {
  shift(w, world, spec, s, {0, 0, -1.5 * spec.unit * dt}, dt);  // 1.5 m/s toward the body
}

// ─── the scripts ─────────────────────────────────────────────────────────────────────────────

MoveCmd run_stop(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = s.t < 1.5 ? 1 : 0;
  return c;
}

MoveCmd forward(const MoveSpec&, const MoveState&, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  return c;
}

MoveCmd sprint(const MoveSpec&, const MoveState&, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.sprint = true;
  return c;
}

MoveCmd standing_jump(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.jump = s.t >= 0.25 && s.t < 1.5;
  return c;
}

MoveCmd run_jump(const MoveSpec&, const MoveState& s, double, ScriptMemory& mem) {
  MoveCmd c;
  c.forward = 1;
  if (s.t >= 1 && mem.jumps == 0 && s.grounded) {
    c.jump = true;
    mem.jumps = 1;
  }
  return c;
}

// Forward and jump pressed afresh on every landing, the view never turned: the air's cap takes
// any gain along the way.
MoveCmd bhop_straight(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.jump = s.t >= 0.5 && s.grounded && !s.jump_held;
  return c;
}

MoveCmd strafe_with(const MoveSpec& spec, const MoveState& s, double dt, ScriptMemory& mem,
                    StrafeKeys keys) {
  if (s.t < 0.5) return forward(spec, s, dt, mem);
  if (s.grounded) mem.side = -mem.side;  // a new side each take-off, as a player swings
  return strafe_cmd(spec, s, keys, mem.side, dt);
}

MoveCmd strafe_fwd(const MoveSpec& spec, const MoveState& s, double dt, ScriptMemory& mem) {
  return strafe_with(spec, s, dt, mem, StrafeKeys::forward_side);
}

MoveCmd strafe_side(const MoveSpec& spec, const MoveState& s, double dt, ScriptMemory& mem) {
  return strafe_with(spec, s, dt, mem, StrafeKeys::side_only);
}

MoveCmd crouch_through(const MoveSpec& spec, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  const double z = s.pos.z / spec.unit;
  c.crouch = z > 1.5 && z < 6.5;
  return c;
}

// Walk off the ledge, then press jump `0.06 s` after the feet left the ground.
MoveCmd coyote(const MoveSpec&, const MoveState& s, double, ScriptMemory& mem) {
  MoveCmd c;
  c.forward = 1;
  if (!s.grounded && mem.mark < 0 && mem.jumps == 0) mem.mark = s.t;
  if (mem.mark >= 0 && mem.jumps == 0 && s.t >= mem.mark + 0.06) {
    c.jump = true;
    mem.jumps = 1;
  } else if (mem.jumps == 1 && s.vel.y > 0) {
    c.jump = true;  // held through the rise
  }
  return c;
}

MoveCmd jump_tap(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.jump = s.t >= 0.25 && s.t < 0.32;
  return c;
}

MoveCmd jump_held(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.jump = s.t >= 0.25 && s.t < 2;
  return c;
}

MoveCmd double_jump(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.jump = (s.t >= 0.25 && s.t < 0.5) || (s.t >= 0.6 && s.t < 1.2);
  return c;
}

MoveCmd slide(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = s.t < 1.2 ? 1 : 0;
  c.sprint = true;
  c.crouch = s.t >= 1.2;
  return c;
}

// Jump running, then turn the view a quarter round over half a second with only forward held.
MoveCmd air_turn(const MoveSpec&, const MoveState& s, double, ScriptMemory& mem) {
  MoveCmd c;
  c.forward = 1;
  if (s.t >= 0.8 && mem.jumps == 0 && s.grounded) {
    c.jump = true;
    mem.jumps = 1;
    mem.mark = s.t;
  }
  if (mem.mark >= 0) c.yaw = core::clamp((s.t - mem.mark) * core::kPi, 0, core::kPi / 2);
  return c;
}

MoveCmd still(const MoveSpec&, const MoveState&, double, ScriptMemory&) { return {}; }

MoveCmd ride_jump(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.jump = s.t >= 1.5 && s.t < 1.6;  // one jump off the moving platform
  return c;
}

MoveCmd ride_walk_off(const MoveSpec&, const MoveState& s, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = s.t >= 4 ? 1 : 0;  // wait for the lift, then walk off it onto the ledge
  return c;
}

MoveCmd stroll(const MoveSpec&, const MoveState&, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.walk = true;
  return c;
}

MoveCmd fly_up(const MoveSpec&, const MoveState&, double, ScriptMemory&) {
  MoveCmd c;
  c.forward = 1;
  c.pitch = 0.5;
  return c;
}

const MoveScenario kScenarios[] = {
    {"run-stop", "classic_arena",
     "forward from rest for 1.5 s, then let go: the run-up and the stop", 3, flat, 0, 0, 0,
     run_stop},
    {"standing-jump", "classic_arena", "one jump from standing: the apex is jump^2 / 2g", 1.5, flat,
     0, 0, 0, standing_jump},
    {"run-jump", "classic_arena", "a jump at full run: how far one jump carries", 2.5, flat, 0, 0,
     0, run_jump},
    {"bhop-straight", "classic_arena",
     "forward and jump on every landing, never turning: no speed gained past the run", 6, flat, 0,
     0, 0, bhop_straight},
    {"strafe-jump", "classic_arena",
     "the strafe jumper: forward and a strafe, the view turned to the best angle, a new side each "
     "jump: speed grows",
     8, flat, 0, 0, 0, strafe_fwd},
    {"strafe-jump-side", "strafe_arena",
     "the strafe jumper on the strafe-only air rules: only a strafe held, the best angle", 8, flat,
     0, 0, 0, strafe_side},
    {"stairs", "classic_arena", "walking up eight steps of 0.25 m: every one climbed, no jump", 1.5,
     stairs, 0, 0, 0, forward},
    {"ramp-walk", "classic_arena", "up a 30 degree ramp: walkable, walked", 2, ramp_walk, 0, 0, 0,
     forward},
    {"ramp-steep", "classic_arena", "into a 55 degree ramp: too steep to stand on, slid off", 2,
     ramp_steep, 0, 0, 0, forward},
    {"crouch-tunnel", "classic_arena", "crouched under a 1.3 m ceiling, standing again past it",
     2.5, tunnel, 0, 0, 0, crouch_through},
    {"ledge", "classic_arena", "running off a 3 m drop: the edge's friction, the fall, the landing",
     2, ledge, 0, 0, 0, forward},
    {"coyote", "platformer", "jump pressed 0.06 s after running off a ledge: still a jump", 2.5,
     gap, 0, 0, 0, coyote},
    {"jump-tap", "platformer", "a jump let go after 0.07 s: cut short", 1.5, flat, 0, 0, 0,
     jump_tap},
    {"jump-hold", "platformer", "the same jump held: full height", 1.5, flat, 0, 0, 0, jump_held},
    {"double-jump", "platformer", "a jump, then a second in the air", 2, flat, 0, 0, 0,
     double_jump},
    {"slide", "modern_shooter", "a sprint, then crouch: the slide, its boost and its friction", 3,
     flat, 0, 0, 0, slide},
    {"stroll-stairs", "walker", "a person sprinting up the same stairs", 3, stairs, 0, 0, 0,
     sprint},
    {"air-control", "strafe_arena",
     "a running jump, the view turned 90 degrees in the air with only forward held: the path bends "
     "(the run ends before landing)",
     1.45, flat, 0, 0, 0, air_turn},
    {"fly", "classic_arena", "flying forward with the view 0.5 rad up: through the air, no gravity",
     2, flat, 0, 0, 0, fly_up, Mode::fly},
    {"platform-ride", "classic_arena",
     "standing on a platform sliding along at 2 m/s, then a jump: carried, and back down on it "
     "where it left",
     3, platform, 0, 0, 0, ride_jump, Mode::walk, slide_platform},
    {"lift", "classic_arena",
     "standing on a lift rising 3 m at 1 m/s, then walking off it onto the ledge at its top", 5,
     lift, 0, 0, 0, ride_walk_off, Mode::walk, raise_lift},
    {"knockback", "classic_arena", "standing, blown up and forward by a blast at 0.5 s: the arc",
     2.5, flat, 0, 0, 0, still, Mode::walk, knock},
    {"jump-pad", "classic_arena",
     "running onto a pad at 3 m that throws the body onto a platform 3 m up at 13 m", 3, pad, 0, 0,
     0, forward, Mode::walk, jump_pad},
    {"trampoline", "classic_arena",
     "walking off a 3 m drop onto a trampoline that gives back 0.8 of each landing: bounce, "
     "bounce, "
     "settle",
     4, trampoline, 0, 0, 0, stroll},
    {"ladder", "classic_arena", "walking into a 4 m ladder: up it, and off its top onto the ledge",
     2.5, ladder, 0, 0, 0, forward},
    {"pusher", "classic_arena", "standing as a wall comes on at 1.5 m/s: pushed along before it", 3,
     pusher, 0, 0, 0, still, Mode::walk, push_wall},
};

void mix(uint64_t& h, double v) {
  uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  for (int i = 0; i < 8; i++) {
    h ^= (u >> (8 * i)) & 0xffu;
    h *= 0x100000001b3ull;
  }
}

}  // namespace

std::span<const MoveScenario> move_scenarios() { return kScenarios; }

const MoveScenario* find_move_scenario(const char* id) {
  for (const MoveScenario& sc : kScenarios) {
    if (std::strcmp(sc.id, id) == 0) return &sc;
  }
  return nullptr;
}

BoxWorld scenario_world(const MoveScenario& sc, const MoveSpec& spec) {
  BoxWorld w;
  w.skin = 0.03125 / 32 * spec.unit;  // a thirty-second of a map unit, whatever the unit
  sc.build(w, spec.unit);
  return w;
}

MoveRun run_move_scenario(const MoveScenario& sc, const MoveSpec& spec, double hz, OnStep on_step,
                          void* ctx) {
  BoxWorld bw = scenario_world(sc, spec);
  const World world = as_world(bw);
  const double dt = 1.0 / hz;
  MoveState s = stand_at(spec, world, {sc.start_x * spec.unit, 0, sc.start_z * spec.unit});
  s.mode = sc.mode;
  const double y0 = s.pos.y;
  ScriptMemory mem;
  MoveRun r;
  r.digest = 0xcbf29ce484222325ull;
  const int n = static_cast<int>(core::num::round(sc.seconds * hz));
  for (int i = 0; i < n; i++) {
    if (sc.animate != nullptr) sc.animate(bw, world, spec, s, dt, mem);
    MoveCmd cmd = sc.script(spec, s, dt, mem);
    if (cmd.yaw == 0 && sc.start_yaw != 0) cmd.yaw = sc.start_yaw;
    const Vec3 before = s.pos;
    const MoveEvents ev = step_move(spec, world, s, cmd, dt);
    r.distance += flat_length(sub(s.pos, before));
    const double speed = flat_length(s.vel);
    if (speed > r.top_speed) r.top_speed = speed;
    if (s.pos.y - y0 > r.apex) r.apex = s.pos.y - y0;
    if (ev.has(kJumped)) r.jumps++;
    if (ev.has(kAirJumped)) r.air_jumps++;
    if (ev.has(kLanded)) {
      r.landings++;
      if (ev.impact > r.max_impact) r.max_impact = ev.impact;
    }
    if (ev.has(kSteppedUp)) r.steps_up++;
    if (ev.has(kSlideStart)) r.slides++;
    if (ev.has(kBounced)) r.bounces++;
    if (ev.has(kLadderOn)) r.climbs++;
    mix(r.digest, s.pos.x);
    mix(r.digest, s.pos.y);
    mix(r.digest, s.pos.z);
    mix(r.digest, s.vel.x);
    mix(r.digest, s.vel.y);
    mix(r.digest, s.vel.z);
    if (on_step != nullptr) on_step(ctx, s, cmd, ev);
  }
  r.steps = n;
  r.end_speed = flat_length(s.vel);
  r.end_height = s.pos.y - y0;
  return r;
}

}  // namespace ge::move
