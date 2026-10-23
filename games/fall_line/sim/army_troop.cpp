// A soldier on his skis (army_troop.hpp).
//
// ge:summary go_toward, stop_here, slope_along and watch_progress's bodies
// ge:keywords army troop go_toward stop_here watch_progress way_toward keep-clear
// ge:skill sim
// ge:test tests/fall_line_army_test.cpp
#include "fall_line/sim/army_troop.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

#include "fall_line/sim/army_rules.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace lm = ge::core::libm;
namespace num = ge::core::num;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;

namespace {

// The fall line's steepness at (x, z), a tangent, and the heading straight up it, rad.
struct FallLine {
  double tan = 0;
  double up = 0;
};
FallLine fall_line(const mg::Level& level, double x, double z) {
  mg::Vec3 n{};
  level.normal_at(x, z, n);
  const double h = hypot(n.x, n.z);
  return FallLine{h / num::max(0.2, n.y), lm::atan2(-n.x, -n.z)};
}

// THE WAY HE TAKES toward (tx, tz): straight at it, but up a rise too steep to climb straight,
// across it (the traverse), and turned off the trunks ahead of him and the soldiers beside him.
double way_toward(const TroopWorld& w, const Soldier& s, double tx, double tz) {
  const mg::Level& level = *w.level;
  double want = lm::atan2(tx - s.x, tz - s.z);
  // A side-step out of where he is caught.
  if (ge::ai::detouring(s.progress, w.t)) want += s.progress.side * kPi / 2;
  const FallLine f = fall_line(level, s.x, s.z);
  const double off_up = angle_diff(f.up, want);
  const double along = f.tan * lm::cos(off_up);
  // The last few metres he sidesteps straight up to it.
  const bool far = hypot(tx - s.x, tz - s.z) > 3 * army::kTurnM;
  if (far && along > army::kTraverse && f.tan > 0.01) {
    // THE TRAVERSE: up the fall line turned off it far enough that the way across climbs at
    // most `kTraverse`, to the side he is headed, turned the other way every `kZigS`.
    const double off = lm::acos(clamp(0.8 * army::kTraverse / f.tan, 0, 1));
    const double zig = num::mod(std::floor((w.t + s.id * 1.7) / army::kZigS), 2) == 0 ? 1.0 : -1.0;
    const double toward = off_up > 0 ? 1.0 : off_up < 0 ? -1.0 : zig;
    want = f.up + off * (std::fabs(off_up) < 0.3 ? zig : toward);
  }
  // Off the trunks ahead of him: each within his look pushes his way to the side it is not on,
  // harder the nearer and the straighter ahead.
  const double fx = lm::sin(want);
  const double fz = lm::cos(want);
  const double ahead = army::kAheadM;
  double push = 0;
  sk::for_each_tree_near(*w.trees, s.x + fx * ahead * 0.5, s.z + fz * ahead * 0.5, [&](int32_t i) {
    const mg::TreeDef& u = level.trees[static_cast<size_t>(i)];
    const double dx = u.x - s.x;
    const double dz = u.z - s.z;
    const double fwd = dx * fx + dz * fz;
    if (fwd <= 0 || fwd > ahead) return;
    const double side = dx * fz - dz * fx;
    const double room = u.radius + army::kSolidM + 0.4;
    if (std::fabs(side) > room) return;
    const double k = (1 - fwd / (ahead + 1)) * (1 - std::fabs(side) / (room + 0.01));
    push += (side >= 0 ? -1 : 1) * k;
  });
  want += clamp(push, -1.2, 1.2);
  // ...and off the other soldiers beside him: those near him in the step's grid when it has one
  // (in the same order as all of them, so the same push to the bit), all of them when not.
  ge::ai::Away away;
  const double apart = army::kApartM;
  const auto from = [&](const Soldier& o) {
    if (o.id == s.id || o.mode == SoldierMode::ride || o.mode == SoldierMode::gone) return;
    ge::ai::add_apart(away, s.x - o.x, s.z - o.z, apart);
  };
  if (w.near && w.near->size() == static_cast<int32_t>(w.all.size())) {
    w.near->for_each_near(s.x, s.z, [&](int32_t i) { from(w.all[static_cast<size_t>(i)]); });
  } else {
    for (const Soldier& o : w.all) from(o);
  }
  const double ax = away.x;
  const double az = away.z;
  if (ax != 0 || az != 0) want = lm::atan2(lm::sin(want) + ax, lm::cos(want) + az);
  return want;
}

// On along his heading at his speed; out of a trunk he touches; inside the map; on the snow.
void move(const TroopWorld& w, Soldier& s) {
  const mg::Level& level = *w.level;
  const double vx = lm::sin(s.heading) * s.speed;
  const double vz = lm::cos(s.heading) * s.speed;
  s.x += vx * w.dt;
  s.z += vz * w.dt;
  sk::for_each_tree_near(*w.trees, s.x, s.z, [&](int32_t i) {
    const mg::TreeDef& u = level.trees[static_cast<size_t>(i)];
    const double dx = s.x - u.x;
    const double dz = s.z - u.z;
    const double d = hypot(dx, dz);
    const double r = u.radius + 0.4;
    if (d >= r) return;
    const double nx = d > 1e-4 ? dx / d : 1;
    const double nz = d > 1e-4 ? dz / d : 0;
    s.x = u.x + nx * r;
    s.z = u.z + nz * r;
    // The way into it taken off him.
    const double into = -(vx * nx + vz * nz);
    if (into > 0) s.speed = num::max(0.0, s.speed - into);
  });
  s.x = clamp(s.x, army::kEdgeM, level.size - army::kEdgeM);
  s.z = clamp(s.z, army::kEdgeM, level.size - army::kEdgeM);
  s.y = level.ground_at(s.x, s.z);
}

}  // namespace

void build_near(ge::ai::Neighbours& near, std::span<const Soldier> all) {
  near.build(
      static_cast<int32_t>(all.size()), army::kApartM + army::kNearSlackM,
      [&](int32_t i) { return all[static_cast<size_t>(i)]; },
      [&](int32_t i) {
        const SoldierMode m = all[static_cast<size_t>(i)].mode;
        return m == SoldierMode::ride || m == SoldierMode::out || m == SoldierMode::rope ||
               m == SoldierMode::board;
      });
}

double slope_along(const mg::Level& level, double x, double z, double heading) {
  mg::Vec3 n{};
  level.normal_at(x, z, n);
  return -(n.x * lm::sin(heading) + n.z * lm::cos(heading)) / num::max(0.2, n.y);
}

void go_toward(const TroopWorld& w, Soldier& s, double tx, double tz, double pace, double stop) {
  const double dt = w.dt;
  const double want = way_toward(w, s, tx, tz);
  const double dist = hypot(tx - s.x, tz - s.z);
  // Turned no tighter than his skis turn at his speed; at a walk, stepped round on the spot.
  const double most = s.speed > 4 ? num::max(1.2, s.speed / army::kTurnM) : 6.0;
  const double turn = clamp(angle_diff(s.heading, want), -most * dt, most * dt);
  s.heading = lm::atan2(lm::sin(s.heading + turn), lm::cos(s.heading + turn));
  const double tan = slope_along(*w.level, s.x, s.z, s.heading);
  const double cap =
      stop >= 0 ? num::min(pace, std::sqrt(num::max(0.0, 2 * army::kBrake * (dist - stop)))) : pace;
  const double sin = tan / std::sqrt(1 + tan * tan);
  double v = s.speed - (army::kGravity * sin + (s.speed > 0.2 ? army::kSnowDrag : 0) +
                        army::kAirDrag * s.speed * s.speed) *
                           dt;
  // A hard turn at speed is skidded: it scrubs.
  v -= std::fabs(turn / dt) * 0.25 * num::min(1.0, s.speed / 10) * dt * s.speed;
  if (v > cap) v = num::max(cap, v - army::kBrake * dt);
  // WORKING: skating, the herringbone, the traverse on skins.
  const double work =
      clamp(army::kSkate - army::kClimb * num::max(0.0, tan), army::kLeast, army::kSkate);
  const double reach = num::min(work, cap);
  s.climbing = v < reach - 0.05;
  if (s.climbing) v = num::min(reach, num::max(v, s.speed) + army::kPush * dt);
  s.speed = num::max(0.0, v);
  move(w, s);
}

void stop_here(const TroopWorld& w, Soldier& s) {
  s.speed = num::max(0.0, s.speed - army::kBrake * 1.5 * w.dt);
  s.climbing = false;
  move(w, s);
}

int32_t watch_progress(Soldier& s, double t, bool moving) {
  ge::ai::Progress& p = s.progress;
  if (!moving) {
    ge::ai::restart(p, s.x, s.z, t);
    return 0;
  }
  if (ge::ai::caught(p, s.x, s.z, t, army::kWindowS, army::kLeastM)) {
    p.side = p.slow % 2 == 1 ? (s.id % 2 == 0 ? 1 : -1) : -p.side;
    p.until = t + army::kDetourS;
  }
  return p.slow;
}

}  // namespace fall_line::sim
