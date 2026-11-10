// The box's look ahead (look_ahead.hpp): the hop's arc, the walk's straight line, the walk's keys
// flown through the move.
//
// ge:summary The box's look ahead: the hop's arc, the walk's line, the walk's keys flown
// ge:keywords look-ahead foresee foresee_walk walk_clear arc_clear hop_turn_rate bumps
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooter/look_ahead.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/framemove.hpp"
#include "shooter/pmove.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

float flat_speed(Vec3f v) { return root_of(v.x * v.x + v.y * v.y); }

float flat_dist(Vec3f a, Vec3f b) { return flat_speed(b - a); }

float yaw_to(Vec3f from, Vec3f to) {
  const double r =
      core::libm::atan2(static_cast<double>(to.y - from.y), static_cast<double>(to.x - from.x));
  return static_cast<float>(r * (180.0 / core::kPi));
}

// A wall: a plane neither floor nor ceiling.
bool wall(const HullTrace& tr) { return tr.plane.normal.z < 0.7f && tr.plane.normal.z > -0.7f; }

// The keys of `way` from `at` moving `v`, toward `aim`.
WalkKeys way_keys(WalkWay way, Vec3f at, Vec3f v, Vec3f aim, float maxspeed) {
  WalkKeys k;
  k.yaw = yaw_to(at, aim);
  k.forward = way == WalkWay::ease ? 0.f : 1.f;
  if (way == WalkWay::brake) {
    // The velocity's part across the way to the aim pushed against: the old way's speed taken off.
    const ViewAxes ax = angle_vectors({0, k.yaw, 0});
    const float s = -dot(v, ax.right) / maxspeed * 1.5f;
    k.side = s > 1 ? 1.f : s < -1 ? -1.f : s;
  }
  return k;
}

WalkSeen fly(const MoveWorld& world, const WalkStart& st, WalkWay way, Vec3f aim) {
  MoveSettings move;
  move.maxspeed = st.maxspeed;
  MoveState m;
  m.origin = st.origin;
  m.velocity = st.velocity;
  m.ground = st.onground ? 0 : -1;
  m.waterlevel = st.waterlevel;
  m.maxspeed = st.maxspeed;
  m.jump_held = true;  // never a jump in the flight
  WalkSeen out;
  const float d0 = flat_dist(st.origin, aim);
  const float step = static_cast<float>(kWalkMs) * 0.001f;
  float t = 0;
  while (t < st.horizon) {
    const WalkKeys k =
        t < st.delay ? st.pressed : way_keys(way, m.origin, m.velocity, aim, st.maxspeed);
    MoveCmd c;
    c.msec = static_cast<uint8_t>(kWalkMs);
    c.angles = {0, k.yaw, 0};
    c.forward = quantize_axis(k.forward * st.maxspeed);
    c.side = quantize_axis(k.side * st.maxspeed);
    const float before = flat_speed(m.velocity);
    const MoveResult r = player_move(m, c, move, world);
    const float lost = before - flat_speed(m.velocity);
    if ((r.blocked & (kBlockedStep | kBlockedOther)) != 0 && lost >= kWalkBump) out.bump += lost;
    t += step;
  }
  out.nearer = d0 - flat_dist(m.origin, aim);
  return out;
}

}  // namespace

// The most the velocity turns a second in the air with the wish across it (hops.hpp's rule:
// 30 units/s across it a command), degrees/s.
float hop_turn_rate(float speed, float dt) {
  if (speed <= 1 || dt <= 0) return 0;
  const double r = core::libm::atan2(30.0, static_cast<double>(speed));
  return static_cast<float>(r * (180.0 / core::kPi)) / dt;
}

// How long the hop's arc on `side` (-1 left, 1 right) stays clear of walls, up to `horizon`: the
// velocity turned at the move's most each command, the player's box traced along it a step up
// (so a stair's riser is not a wall). Floors and ramps it meets do not count.
float arc_clear(const MoveWorld& world, Vec3f o, Vec3f v, int side, float dt, float horizon) {
  const float speed = root_of(v.x * v.x + v.y * v.y);
  if (speed < 1 || dt <= 0) return horizon;
  const double step =
      -static_cast<double>(side) * hop_turn_rate(speed, dt) * dt * (core::kPi / 180.0);
  const auto c = static_cast<float>(core::libm::cos(step)),
             sn = static_cast<float>(core::libm::sin(step));
  Vec3f at{o.x, o.y, o.z + kStepHeight};
  if (!box_free(world, at)) at = o;
  Vec3f vel{v.x, v.y, 0};
  constexpr int kChunk = 4;  // commands a trace covers
  for (float t = 0; t < horizon;) {
    Vec3f to = at;
    for (int i = 0; i < kChunk; i++) {
      vel = {vel.x * c - vel.y * sn, vel.x * sn + vel.y * c, 0};
      to = to + vel * dt;
    }
    const HullTrace tr = player_trace(world, at, to);
    if (tr.startsolid) return t;
    if (tr.fraction < 1 && tr.plane.normal.z < 0.7f) return t + tr.fraction * dt * kChunk;
    at = to;
    t += dt * kChunk;
  }
  return horizon;
}

bool walk_clear(const MoveWorld& world, Vec3f o, Vec3f to) {
  Vec3f a{o.x, o.y, o.z + kStepHeight};
  if (!box_free(world, a)) a = o;
  const HullTrace tr = player_trace(world, a, {to.x, to.y, to.z + kStepHeight});
  return !tr.startsolid && (tr.fraction >= 1 || !wall(tr));
}

const char* walk_way_name(WalkWay way) {
  static constexpr const char* kNames[kWalkWays] = {"pursue", "brake", "near", "ease"};
  return kNames[static_cast<int>(way)];
}

const char* walk_way_because(WalkWay way) {
  switch (way) {
    case WalkWay::brake:
      return "walking the route: braking into the turn foreseen";
    case WalkWay::near:
      return "walking the route: the near node, a bump foreseen";
    case WalkWay::ease:
      return "walking the route: easing off, a bump foreseen";
    default:
      return "walking the route";
  }
}

WalkWay foresee_walk(const MoveWorld& world, const WalkStart& st, Vec3f aim, Vec3f near,
                     WalkKeys& keys, WalkSeen seen[kWalkWays], bool& flown) {
  flown = false;
  for (int k = 0; k < kWalkWays; k++) seen[k] = {};
  if (flat_speed(st.velocity) < 150) return WalkWay::pursue;
  {
    // The cheap look first: the box swept straight along the velocity, a step up.
    Vec3f a{st.origin.x, st.origin.y, st.origin.z + kStepHeight};
    if (!box_free(world, a)) a = st.origin;
    const HullTrace tr = player_trace(
        world, a, {a.x + st.velocity.x * st.horizon, a.y + st.velocity.y * st.horizon, a.z});
    if (tr.fraction >= 1 || !wall(tr)) return WalkWay::pursue;
  }
  seen[0] = fly(world, st, WalkWay::pursue, aim);
  if (seen[0].bump <= 0) return WalkWay::pursue;
  flown = true;
  int best = 0;
  for (int k = 1; k < kWalkWays; k++) {
    const auto way = static_cast<WalkWay>(k);
    seen[k] = fly(world, st, way, way == WalkWay::near ? near : aim);
    const bool clean = seen[k].bump <= 0, best_clean = seen[best].bump <= 0;
    if (clean != best_clean
            ? clean
            : (clean ? seen[k].nearer > seen[best].nearer : seen[k].bump < seen[best].bump))
      best = k;
  }
  const auto chosen = static_cast<WalkWay>(best);
  if (chosen != WalkWay::pursue)
    keys =
        way_keys(chosen, st.origin, st.velocity, chosen == WalkWay::near ? near : aim, st.maxspeed);
  return chosen;
}

void record_walk(ai::Why* why, float speed, WalkWay way, const WalkSeen seen[kWalkWays]) {
  if (why == nullptr) return;
  why->decide("foresee");
  why->fact("speed", static_cast<double>(speed));
  for (int k = 0; k < kWalkWays; k++) {
    why->option(walk_way_name(static_cast<WalkWay>(k)),
                static_cast<double>(seen[k].nearer - seen[k].bump));
    why->term("bump", static_cast<double>(seen[k].bump));
    why->term("nearer", static_cast<double>(seen[k].nearer));
  }
  why->choose(walk_way_name(way), way == WalkWay::pursue
                                      ? "every way bumps: the pursuit's"
                                      : "the pursuit bumps: a way that does not");
}

}  // namespace ge::shooter
