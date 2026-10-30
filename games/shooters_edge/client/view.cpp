// The player's view of the listen server (view.hpp).
//
// ge:summary The HUD's status and the camera's subject read off the listen server
// ge:keywords status_of subject_of engine_point engine_heading engine_pitch
// ge:skill shooter camera ui
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/view.hpp"

#include <cmath>

#include "brush/map.hpp"
#include "shooter/brush_hull.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The mod's weapon bits (games/shooters_edge/qc/defs.qc's W_*) as the HUD's weapons, in its order.
constexpr int kWeaponBits[hud::kGuns] = {1, 2, 4, 8, 16, 32, 64};
constexpr int kBlade = 4096;
constexpr int kArmour1 = 8192, kArmour2 = 16384, kArmour3 = 32768;

int bits(float f) { return f > 0 ? static_cast<int>(f) : 0; }

}  // namespace

ge::camera::Vec3 engine_point(sh::Vec3f p) {
  const float f[3] = {p.x, p.y, p.z};
  double e[3];
  ge::brush::to_engine(f, e);
  return {e[0], e[1], e[2]};
}

double engine_heading(float yaw_deg) { return kPi / 2 - static_cast<double>(yaw_deg) * kDeg; }

double engine_pitch(float pitch_deg) { return -static_cast<double>(pitch_deg) * kDeg; }

hud::Status status_of(const Listen& l, double t) {
  hud::Status s;
  s.t = t;
  const PlayerState p = player_state(l);
  s.health = static_cast<int>(std::ceil(p.health));
  s.dead = p.dead;
  s.armour = static_cast<int>(field_named(l, 1, "armorvalue"));
  const int items = bits(field_named(l, 1, "items"));
  s.armour_class = (items & kArmour3) != 0   ? hud::ArmourClass::heavy
                   : (items & kArmour2) != 0 ? hud::ArmourClass::medium
                   : (items & kArmour1) != 0 ? hud::ArmourClass::light
                                             : hud::ArmourClass::none;
  s.ammo = {static_cast<int>(field_named(l, 1, "ammo_shells")),
            static_cast<int>(field_named(l, 1, "ammo_nails")),
            static_cast<int>(field_named(l, 1, "ammo_rockets")),
            static_cast<int>(field_named(l, 1, "ammo_cells"))};
  s.owned = 0;
  for (int k = 0; k < hud::kGuns; k++)
    if ((items & kWeaponBits[k]) != 0) s.owned = static_cast<uint8_t>(s.owned | (1u << k));
  const int held = bits(field_named(l, 1, "weapon"));
  s.weapon = hud::Weapon::melee;
  if (held != kBlade)
    for (int k = 0; k < hud::kGuns; k++)
      if (held == kWeaponBits[k]) s.weapon = static_cast<hud::Weapon>(k + 1);
  s.frags = static_cast<int>(field_named(l, 1, "frags"));
  return s;
}

ge::camera::Subject subject_of(const Listen& l, ge::camera::FirstPersonRow& row) {
  const PlayerState p = player_state(l);
  ge::camera::Subject s;
  // The feet: the player's box reaches `kPlayerMins.z` under his origin.
  sh::Vec3f feet = p.origin;
  feet.z += ge::shooter::kPlayerMins.z;
  s.pos = engine_point(feet);
  const ge::camera::Vec3 v = engine_point(p.velocity);
  s.vel = v;
  s.grounded = p.on_ground;
  s.look_yaw = engine_heading(l.view.y);
  s.look_pitch = engine_pitch(l.view.x);
  s.heading = s.look_yaw;
  row.eye_stand_m =
      (static_cast<double>(p.view_ofs.z) - ge::shooter::kPlayerMins.z) * ge::brush::kUnitM;
  return s;
}

}  // namespace shooters_edge::client
