// The free ride's snowmobile (sled_ride.hpp).
//
// ge:summary sled_spot, place_sled, step_sled_ride, board, hop, throw off, remount
// ge:keywords sled snowmobile spot board hop throw remount hold boards asleep settle
// ge:skill sim snowmobile
// ge:test tests/fall_line_sled_test.cpp
#include "fall_line/sim/sled_ride.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/quat.hpp"
#include "fall_line/sim/course.hpp"
#include "skier/skier.hpp"
#include "snowmobile/spec.hpp"

namespace fall_line::sim {

namespace {

namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
namespace sk = ge::skier;
namespace sm = ge::snowmobile;
namespace core = ge::core;
namespace num = ge::core::num;
namespace lm = ge::core::libm;
using namespace sled_ride;

// The ground's steepest lean over a circle of radius `r`, rise over run.
double lean_over(const mg::Level& level, double x, double z, double r) {
  const double g = level.ground_at(x, z);
  double most = 0;
  for (int i = 0; i < 8; i++) {
    const double a = static_cast<double>(i) / 8 * 2 * core::kPi;
    most =
        num::max(most, std::fabs(level.ground_at(x + lm::sin(a) * r, z + lm::cos(a) * r) - g) / r);
  }
  return most;
}

bool clear_at(const mg::Level& level, const std::vector<ml::LiftPlan>& lifts,
              const sk::TreeIndex& trees, double x, double z) {
  constexpr double room = kPadRadius;
  bool tree = false;
  sk::for_each_tree_near(trees, x, z, [&](int32_t i) {
    const mg::TreeDef& t = level.trees[static_cast<size_t>(i)];
    if (core::hypot(t.x - x, t.z - z) < room + 2 + t.radius) tree = true;
  });
  if (tree) return false;
  for (const ml::LiftPlan& p : lifts) {
    const double rx = x - p.bottom.x;
    const double rz = z - p.bottom.z;
    if (core::hypot(rx, rz) < kStationGap + room) return false;
    const double u = rx * p.dx + rz * p.dz;
    const double v = rx * p.dz - rz * p.dx;
    if (u > -room && u < p.length + room && std::fabs(v) < kLineGap + room) return false;
  }
  if (level.resort) {
    for (const mg::WindTunnel& t : level.resort->tunnels) {
      for (const mg::WindTunnel::Point& q : t.points) {
        if (core::hypot(q.x - x, q.z - z) < kTunnelGap + room + t.width / 2) return false;
      }
    }
  }
  return true;
}

// The machine stood back on its belt where it is, at rest, its engine as it was.
void restand(const mg::Level& level, FreeSled& m) {
  const double rpm = m.body.rpm;
  const bool running = m.body.running;
  m.body = sm::stand_sled(level, m.body.x, m.body.z, m.body.heading);
  m.body.rpm = rpm;
  m.body.running = running;
}

void say(GameState& state, int32_t i, SledPhase phase, double speed) {
  state.events.push_back(GameEvent{EventKind::sled, i, static_cast<int32_t>(phase), speed});
}

// THE SKIER ON THE BOARDS: where the machine has him, facing its way, moving with it. His skis are
// on the rack: nothing of theirs is edged, tucked or sunk.
void hold(GameState& state, int32_t i) {
  sk::SkierState& c = state.racers[static_cast<size_t>(i)].skier;
  const sm::SledState& b = state.sled.body;
  const core::Vec3 at = sm::boards_at(b);
  c.x = at.x;
  c.y = at.y;
  c.z = at.z;
  c.vx = b.vx;
  c.vy = b.vy;
  c.vz = b.vz;
  c.heading = b.heading;
  c.yaw_rate = b.wy;
  c.speed = b.speed;
  c.on_snow = !b.airborne;
  c.air_time = 0;
  c.air_peak = 0;
  c.edge = 0;
  c.tuck = 0;
  c.lean = 0;
  c.sink = 0;
  c.g_lat = 0;
  c.jump_held = 0;
  c.down = false;
  c.down_time = 0;
  c.cause = sk::Wipeout::none;
}

// On the boards, the skis racked: the machine his, the engine started.
void mount(GameState& state, int32_t i) {
  FreeSled& m = state.sled;
  Racer& r = state.racers[static_cast<size_t>(i)];
  m.rider = true;
  m.mode = SledMode::ridden;
  m.t = 0;
  m.still = 0;
  m.asleep = false;
  m.body.running = true;
  m.body.rpm = num::max(m.body.rpm, sm::spec::kIdleRpm);
  r.free.lift.phase = LiftPhase::none;
  r.free.lift.carrier = -1;
  r.progress.stuck = 0;
  hold(state, i);
}

void board(GameState& state, int32_t i) {
  FreeSled& m = state.sled;
  if (m.body.over_for > 0 || m.mode == SledMode::down) restand(*state.level, m);
  m.taken++;
  mount(state, i);
  say(state, i, SledPhase::board, m.body.speed);
}

// THE HOP OFF: stood beside it on its left, the rack's side, with its way, a little out and (if
// it was moving) up; slow, facing down the fall line. A new descent: his foot is to be reached
// again.
void hop(GameState& state, int32_t i) {
  FreeSled& m = state.sled;
  const sm::SledState& b = m.body;
  Racer& r = state.racers[static_cast<size_t>(i)];
  const core::Vec3 left = core::rotate(b.q, core::Vec3{-1, 0, 0});
  const double x = b.x + left.x * kStandOff;
  const double z = b.z + left.z * kStandOff;
  const double flat = core::hypot(b.vx, b.vz);
  double heading = b.heading;
  if (flat > kTurnDown) {
    heading = lm::atan2(b.vx, b.vz);
  } else {
    mg::Vec3 n{0, 1, 0};
    state.level->normal_at(x, z, n);
    if (core::hypot(n.x, n.z) > 1e-3) heading = lm::atan2(n.x, n.z);
  }
  r.skier = sk::stand_skier(state.world(), x, z, heading, 0);
  const double up = b.speed > kHopMoving ? kHopUp : 0;
  r.skier.vx = b.vx + left.x * kHopOut;
  r.skier.vz = b.vz + left.z * kHopOut;
  r.skier.vy = num::max(0.0, b.vy) + up;
  r.skier.speed = core::hypot3(r.skier.vx, r.skier.vy, r.skier.vz);
  r.free.arrived = false;
  r.progress.stuck = 0;
  m.rider = false;
  m.mode = SledMode::parked;
  m.t = 0;
  say(state, i, SledPhase::hop, b.speed);
}

// THE RIDER THROWN: put down off the boards with the way the machine had before the step that
// threw him (`v0`), his skis still on its rack; it goes on without him and lies where it comes to
// rest.
void throw_off(GameState& state, int32_t i, const core::Vec3& v0, sm::Thrown why) {
  FreeSled& m = state.sled;
  Racer& r = state.racers[static_cast<size_t>(i)];
  const core::Vec3 at = sm::boards_at(m.body);
  const double over = at.y - state.level->ground_at(at.x, at.z);
  r.skier = sk::stand_skier(state.world(), at.x, at.z, m.body.heading, 0, num::max(0.0, over));
  r.skier.vx = v0.x;
  r.skier.vy = v0.y;
  r.skier.vz = v0.z;
  r.skier.speed = core::hypot3(v0.x, v0.y, v0.z);
  r.skier.down = true;
  r.skier.down_time = 0;
  r.skier.cause = why == sm::Thrown::tree ? sk::Wipeout::tree : sk::Wipeout::landing;
  r.progress.wipeouts++;
  state.events.push_back(
      GameEvent{EventKind::wipeout, i, static_cast<int32_t>(r.skier.cause), r.skier.speed});
  m.rider = false;
  m.mode = SledMode::down;
  m.thrown = true;
  m.t = 0;
  say(state, i, SledPhase::crash, r.skier.speed);
}

// The machine with nobody on it: settling on its parking brake until it is at rest, then asleep;
// the engine shut off once it has idled long enough or lies thrown over.
void step_parked(GameState& state) {
  FreeSled& m = state.sled;
  if (m.body.running && (m.mode == SledMode::down || m.t > kIdleFor)) {
    m.body.running = false;
    m.body.rpm = 0;
  }
  if (m.asleep) return;
  sm::SledEvents ev;
  sm::step_sled(m.body, sm::SledControls{}, 0, false, sled_world(state), kDt, ev);
  const sm::SledState& b = m.body;
  if (m.mode == SledMode::down && b.speed < 0.3 && b.over_for == 0) m.mode = SledMode::parked;
  const bool rest = b.speed < kSleepSpeed && core::hypot3(b.wx, b.wy, b.wz) < kSleepSpeed;
  m.still = rest ? m.still + kDt : 0;
  if (m.still >= kSleepAfter) m.asleep = true;
}

}  // namespace

SledSpot sled_spot(const mg::Level& level, const std::vector<ml::LiftPlan>& lifts,
                   const sk::TreeIndex& trees) {
  const mg::TrackPoint& finish = level.track.points.back();
  const double cx = level.resort ? level.resort->village.x : finish.x;
  const double cz = level.resort ? level.resort->village.z : finish.z;
  const double half = level.size / 2;
  const double edge = kPadRadius + 30;
  bool found = false;
  double bx = 0, bz = 0, best = 0;
  const auto n = static_cast<int>(kSearch / kStep);
  for (int iz = -n; iz <= n; iz++) {
    for (int ix = -n; ix <= n; ix++) {
      const double dx = ix * kStep;
      const double dz = iz * kStep;
      const double x = cx + dx;
      const double z = cz + dz;
      if (std::fabs(x - half) > half - edge || std::fabs(z - half) > half - edge) continue;
      const double d = std::fabs(core::hypot(dx, dz) - kBeside);
      if (found && d >= best) continue;
      const double lean = lean_over(level, x, z, kPadRadius);
      if (lean > kPadSlope) continue;
      const double cost = d + lean * 400;
      if (found && cost >= best) continue;
      if (!clear_at(level, lifts, trees, x, z)) continue;
      found = true;
      bx = x;
      bz = z;
      best = cost;
    }
  }
  if (!found) {
    bx = cx + 12;
    bz = cz;
  }
  // Parked facing up the mountain: the fall line is +z, so the nose to -z.
  return SledSpot{bx, bz, core::kPi};
}

void place_sled(GameState& state) {
  FreeSled& m = state.sled;
  const SledSpot spot = sled_spot(*state.level, *state.lifts, state.trees);
  m = FreeSled{};
  m.present = true;
  m.body = sm::stand_sled(*state.level, spot.x, spot.z, spot.heading);
  const auto steps = static_cast<int>(num::round(kSettle * kHz));
  for (int k = 0; k < steps && !m.asleep; k++) step_parked(state);
  m.asleep = true;
  m.still = 0;
  m.t = 0;
}

sm::SledControls sled_controls(const Controls& c) {
  return sm::SledControls{core::clamp(c.ski.tuck, 0, 1), core::clamp(c.ski.brake, 0, 1),
                          core::clamp(c.ski.steer, -1, 1), core::clamp(c.lean, -1, 1)};
}

sm::SledWorld sled_world(const GameState& state) {
  return sm::SledWorld{state.level.get(), &state.trees, 0, 0, 1};
}

bool sled_within(const GameState& state, int32_t i) {
  const FreeSled& m = state.sled;
  if (i != 0 || !m.present || m.rider || m.thrown) return false;
  const Racer& r = state.racers[static_cast<size_t>(i)];
  const sk::SkierState& c = r.skier;
  if (c.down || r.free.lift.phase != LiftPhase::none) return false;
  return core::hypot(c.x - m.body.x, c.z - m.body.z) <= kBoardReach && c.speed <= kBoardFastest &&
         m.body.speed <= kStill;
}

bool step_sled_ride(GameState& state, int32_t i, const Controls& c) {
  FreeSled& m = state.sled;
  if (!m.present) return false;
  if (i != 0) return false;
  m.t += kDt;
  if (!m.rider) {
    // Taken on: the press that does it is spent on it, never read again as the hop off.
    if (c.machine && sled_within(state, i)) {
      board(state, i);
      return true;
    }
    step_parked(state);
    return false;
  }
  if (c.machine) {
    hop(state, i);
    return false;
  }
  Racer& r = state.racers[static_cast<size_t>(i)];
  const core::Vec3 v0{m.body.vx, m.body.vy, m.body.vz};
  sm::SledEvents ev;
  sm::step_sled(m.body, sled_controls(c), state.spec.mass, true, sled_world(state), kDt, ev);
  if (ev.took_off) state.events.push_back(GameEvent{EventKind::takeoff, i, r.progress.next, 0});
  if (ev.landed) {
    state.events.push_back(GameEvent{EventKind::landing, i, r.progress.next, 1 + ev.into / 3});
  }
  if (ev.hit > 0) state.events.push_back(GameEvent{EventKind::glance, i, ev.tree, 0});
  if (ev.thrown != sm::Thrown::none) {
    throw_off(state, i, v0, ev.thrown);
    return false;
  }
  hold(state, i);
  return true;
}

bool remount_sled(GameState& state, int32_t i) {
  FreeSled& m = state.sled;
  if (i != 0 || !m.present) return false;
  if (m.rider) {
    // RIGHTED AND DUG OUT: his reset on the boards stands the machine back on its belt.
    restand(*state.level, m);
    hold(state, i);
    say(state, i, SledPhase::right, 0);
    return true;
  }
  if (!m.thrown) return false;
  restand(*state.level, m);
  m.thrown = false;
  mount(state, i);
  say(state, i, SledPhase::right, 0);
  return true;
}

const char* sled_phase_name(SledPhase p) {
  switch (p) {
    case SledPhase::board:
      return "board";
    case SledPhase::hop:
      return "hop";
    case SledPhase::crash:
      return "crash";
    case SledPhase::right:
      return "right";
  }
  return "?";
}

}  // namespace fall_line::sim
