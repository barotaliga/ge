// The skier meeting the army (army_meet.hpp).
//
// ge:summary meet_army's body: the hull, the knock off, the throw, a man run down
// ge:keywords army meet_army hull knock-off run-down push-apart closing restitution
// ge:skill sim
// ge:test tests/fall_line_army_meet_test.cpp
#include "fall_line/sim/army_meet.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/army_sled.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "fall_line/sim/wanted.hpp"
#include "skier/skier.hpp"
#include "skier/tuning.hpp"
#include "snowmobile/spec.hpp"

namespace fall_line::sim {

namespace {

namespace sk = ge::skier;
namespace sm = ge::snowmobile;
namespace core = ge::core;
namespace num = ge::core::num;
namespace lm = ge::core::libm;

constexpr double kSkierRadius = sk::tuning::wipeout::kBodyRadius;

// A meeting: how far they overlap (m), the way from the other to him (a unit vector in the plan),
// and the closing speed between them, m/s (positive coming together).
struct Contact {
  double pen = 0;
  double nx = 0, nz = 0;
};

// Push him and the other apart by their masses, `mk` his and `mo` the other's, and trade the
// closing speed back at `e`. Returns the closing speed met, m/s (0 when they were parting).
double push_apart(sk::SkierState& c, double& ox, double& oz, double& ovx, double& ovz,
                  const Contact& k, double mk, double mo, double e) {
  const double share = mo / (mk + mo);
  c.x += k.nx * k.pen * share;
  c.z += k.nz * k.pen * share;
  ox -= k.nx * k.pen * (1 - share);
  oz -= k.nz * k.pen * (1 - share);
  const double closing = -((c.vx - ovx) * k.nx + (c.vz - ovz) * k.nz);
  if (closing <= 0) return 0;
  const double j = (1 + e) * closing / (1 / mk + 1 / mo);
  c.vx += j / mk * k.nx;
  c.vz += j / mk * k.nz;
  ovx -= j / mo * k.nx;
  ovz -= j / mo * k.nz;
  c.speed = core::hypot3(c.vx, c.vy, c.vz);
  return closing;
}

// THROWN: he goes down off his skis, as into a trunk.
void throw_down(GameState& state, sk::SkierState& c) {
  if (c.down) return;
  sk::put_down(c, sk::Wipeout::tree);
  state.racers[0].progress.wipeouts++;
  state.events.push_back(GameEvent{EventKind::wipeout, 0, static_cast<int32_t>(c.cause), c.speed});
}

void meet_machines(GameState& state, sk::SkierState& c, double mk) {
  constexpr double kHalfL = sm::spec::kLength / 2;
  for (ArmySled& m : state.army.sleds) {
    if (m.mode == ArmySledMode::gone) continue;
    sm::SledState& b = m.body;
    if (std::fabs(c.x - b.x) > kHalfL + 2 || std::fabs(c.z - b.z) > kHalfL + 2) continue;
    if (c.y > b.y + army_sled::kHullTopM || c.y < b.y - 2) continue;
    // The nearest point on the machine's spine to him.
    const double fx = lm::sin(b.heading);
    const double fz = lm::cos(b.heading);
    const double u = core::clamp((c.x - b.x) * fx + (c.z - b.z) * fz, -kHalfL, kHalfL);
    const double dx = c.x - (b.x + fx * u);
    const double dz = c.z - (b.z + fz * u);
    const double d = core::hypot(dx, dz);
    const double reach = sm::spec::kHalfWidth + kSkierRadius;
    if (d >= reach) continue;
    // Dead on its spine he is put out to its side he came from.
    const Contact k{reach - d, d > 1e-6 ? dx / d : fz, d > 1e-6 ? dz / d : -fx};
    // His own pace into it, before the push: what knocks a crew over.
    const double into = -(c.vx * k.nx + c.vz * k.nz);
    const bool crew = crewed(m) && m.mode != ArmySledMode::due;
    const double mo = sm::sled_mass(crew ? army_sled::kRiderKg : 0);
    const double closing = push_apart(c, b.x, b.z, b.vx, b.vz, k, mk, mo, army_sled::kRestitution);
    b.speed = core::hypot3(b.vx, b.vy, b.vz);
    if (c.down) continue;
    if (crew && into >= army_sled::kKnockOffSpeed) {
      knock_off(state, m);
      raise_heat(state, WantedCause::hit_soldier);
    }
    if (closing >= army_sled::kThrowSpeed) throw_down(state, c);
  }
}

// Whether a man stands on the snow where a skier meets him.
bool on_foot(const Soldier& s) {
  return s.mode != SoldierMode::ride && s.mode != SoldierMode::rope && s.mode != SoldierMode::gone;
}

void meet_soldiers(GameState& state, sk::SkierState& c, double mk) {
  constexpr double reach = army::kManRadiusM + kSkierRadius;
  int32_t down = 0;
  for (Soldier& s : state.army.soldiers) {
    if (!on_foot(s)) continue;
    if (std::fabs(c.x - s.x) > 2 || std::fabs(c.z - s.z) > 2 || std::fabs(c.y - s.y) > 2) continue;
    const double dx = c.x - s.x;
    const double dz = c.z - s.z;
    const double d = core::hypot(dx, dz);
    if (d >= reach) continue;
    const Contact k{reach - d, d > 1e-6 ? dx / d : 1, d > 1e-6 ? dz / d : 0};
    const double into = -(c.vx * k.nx + c.vz * k.nz);
    double vx = s.speed * lm::sin(s.heading);
    double vz = s.speed * lm::cos(s.heading);
    push_apart(c, s.x, s.z, vx, vz, k, mk, army::kManKg, army_sled::kRestitution);
    s.y = state.level->ground_at(s.x, s.z);
    if (c.down || s.mode == SoldierMode::down || into < army::kRunDownSpeed) continue;
    s.speed = 0;
    enter_mode(state, s, SoldierMode::down, "run down");
    down++;
  }
  if (down == 0) return;
  state.events.push_back(GameEvent{EventKind::army, 0, static_cast<int32_t>(ArmyChange::run_down),
                                   static_cast<double>(down)});
  raise_heat(state, WantedCause::hit_soldier);
}

}  // namespace

void meet_army(GameState& state) {
  if (!state.army.active || state.racers.empty() || !state.sled.present) return;
  Racer& r = state.racers[0];
  if (r.progress.finished || r.body.mortal >= 0) return;
  if (r.free.lift.phase != LiftPhase::none || on_sled(state, 0) || state.heli.rider) return;
  if (state.wanted.ended == WantedEnd::arrested) return;
  const double mk = state.spec.mass;
  meet_machines(state, r.skier, mk);
  meet_soldiers(state, r.skier, mk);
}

}  // namespace fall_line::sim
