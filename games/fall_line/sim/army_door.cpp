// The door gunners (army_door.hpp): the gunners shared out, the doors slid, each gun laid in its
// arc and fired through the gunfire.
//
// ge:summary The door gunners' step: gunners shared out, doors slid, guns laid and fired
// ge:keywords door-gun door-gunner gunners_of doors_manned step_army_doors lay traverse arc pivot
// ge:skill sim
// ge:test tests/fall_line_army_door_test.cpp
#include "fall_line/sim/army_door.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

#include "fall_line/sim/army_heli_rules.hpp"
#include "fall_line/sim/gunfire.hpp"
#include "fall_line/sim/weapons_rules.hpp"

namespace fall_line::sim {

namespace lm = ge::core::libm;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;
using namespace army_heli;

namespace {

// A door: `side` 1 the right, -1 the left; its gun's id and its place in the machine's lay arrays.
int32_t gun_id(const ArmyHeli& h, int32_t side) { return kGunBase + 2 * h.id + (side > 0 ? 0 : 1); }
size_t slot(int32_t side) { return side > 0 ? 0 : 1; }

// THE PIVOT of the gun in the door on `side`, world frame (the machine's heading only: its bank and
// pitch are left out of where the gun stands).
Spot pivot(const ArmyHeli& h, int32_t side) {
  const double fx = lm::sin(h.heading);
  const double fz = lm::cos(h.heading);
  const double out = side * kPivotOutM;
  return Spot{h.x + fz * out + fx * kPivotAheadM, h.y + kPivotUpM,
              h.z - fx * out + fz * kPivotAheadM};
}

// The world bearing of a gun on `side` laid `yaw` forward of square out of its door.
double bearing_of(const ArmyHeli& h, int32_t side, double yaw) {
  return h.heading + side * (kPi / 2 - yaw);
}

void slide(ArmyHeli& h, double open) {
  h.doors = clamp(h.doors + (open > h.doors ? 1 : open < h.doors ? -1 : 0) * 0.6 * kDt, 0, 1);
}

}  // namespace

int32_t gunners_of(const GameState& state, const ArmyHeli& h) {
  if (h.mode != ArmyHeliMode::hunt) return 0;
  int32_t left = roster_of(state.wanted.stars).gunners;
  for (const ArmyHeli& o : state.army.helis) {
    if (&o == &h) break;
    if (o.mode == ArmyHeliMode::hunt) left -= 2;
  }
  return left < 0 ? 0 : left > 2 ? 2 : left;
}

bool doors_manned(const GameState& state, const ArmyHeli& h) {
  if (!state.army.fire || state.wanted.ended != WantedEnd::running) return false;
  if (gunners_of(state, h) == 0) return false;
  return rules_of(state, roster_of(state.wanted.stars).fire) != FireRule::hold;
}

void step_army_doors(GameState& state, Spot q) {
  if (!state.army.fire) return;
  const WantedState& w = state.wanted;
  const FireRule asked = roster_of(w.stars).fire;
  const GunDef& def = weapons::gun_def(WeaponKind::door_gun);
  const bool cover = under_cover(state);
  for (ArmyHeli& h : state.army.helis) {
    const bool manned = doors_manned(state, h);
    const int32_t gunners = manned ? gunners_of(state, h) : 0;
    const double range = hypot(hypot(q.x - h.x, q.z - h.z), q.y - h.y);
    if (h.mode == ArmyHeliMode::hunt) slide(h, manned && w.seen && range < kOpenM ? 1 : 0);
    for (const int32_t side : {1, -1}) {
      // One gunner mans the inward door: the side its circle turns toward.
      const bool on = gunners == 2 || (gunners == 1 && side == h.dir);
      const int32_t id = gun_id(h, side);
      if (!on) {
        if (state.gunfire.active) disarm_gun(state, id);
        continue;
      }
      // THE LAY: onto him where the arc lets it, else back to its rest, at a man's pace.
      const Spot p = pivot(h, side);
      const double dx = q.x - p.x;
      const double dz = q.z - p.z;
      const double yaw = -side * angle_diff(h.heading + side * (kPi / 2), lm::atan2(dx, dz));
      const double pitch = lm::atan2(q.y - p.y, hypot(dx, dz));
      const bool in_arc = yaw <= kArc && yaw >= -kArc && pitch <= kElevate && pitch >= -kDepress;
      const bool bears = w.seen && in_arc;
      const size_t k = slot(side);
      const double turn = kTraverse * kDt;
      h.lay_yaw[k] =
          clamp(h.lay_yaw[k] + clamp((bears ? yaw : 0) - h.lay_yaw[k], -turn, turn), -kArc, kArc);
      h.lay_pitch[k] =
          clamp(h.lay_pitch[k] + clamp((bears ? pitch : kRestPitch) - h.lay_pitch[k], -turn, turn),
                -kDepress, kElevate);
      // The muzzle out along the gun as it is laid.
      const double a = bearing_of(h, side, h.lay_yaw[k]);
      const double flat = kMuzzleM * lm::cos(h.lay_pitch[k]);
      const double mx = p.x + lm::sin(a) * flat;
      const double my = p.y + kMuzzleM * lm::sin(h.lay_pitch[k]);
      const double mz = p.z + lm::cos(a) * flat;
      Gun* g = arm_gun(state, id, WeaponKind::door_gun, mx, my, mz);
      if (g == nullptr) continue;
      g->x = mx;
      g->y = my;
      g->z = mz;
      g->vx = h.vx;
      g->vy = h.vy;
      g->vz = h.vz;
      const bool laid = angle_diff(h.lay_yaw[k], yaw) < kLaid &&
                        angle_diff(h.lay_yaw[k], yaw) > -kLaid && h.lay_pitch[k] - pitch < kLaid &&
                        pitch - h.lay_pitch[k] < kLaid;
      if (!bears || !laid || h.doors < kDoorsOpen || !gun_ready(*g) || range > def.range) continue;
      const Pursuer eye{kPursuerBase + h.id, PursuerKind::helicopter, p.x, p.y, p.z};
      if (!sees_quarry(*state.level, state.trees, eye, q, cover)) continue;
      fire_at(state, *g, AimTarget{}, asked);
    }
  }
}

}  // namespace fall_line::sim
