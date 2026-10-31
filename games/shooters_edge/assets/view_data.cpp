// The `se_view` data set: what Shooter's Edge's held-weapon builders (content/models/view/<name>/
// build.py, over weapon_kit.py) are made from: the built weapons' scale, the shoulders' places from
// the eye, the arm's lengths (view/weapons.hpp), its joints' ranges (viewmodel/arm.hpp) and each
// weapon's offset from the eye, so the kit puts the shoulders where the game's view has them for
// that weapon and keeps the arm inside its ranges, and a change to any of them makes every weapon
// model stale.
//
// ge:summary The held weapons' data: the built scale, the shoulders, the arm, each weapon's offset
// ge:keywords shooters-edge asset data viewmodel weapons shoulders arm offset builder se_view
// ge:skill held-weapons assets render
// ge:test tests/shooters_edge_view_test.cpp
#include <string>

#include "assets/data.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/arm.hpp"

namespace {

namespace se = shooters_edge::view;

std::string make() {
  ge::assets::JsonOut j;
  j.begin_object();
  j.field("frame", "view: x right, y up, z forward, m from the eye; true metres unless said");
  j.field("scale", se::kBuiltScale);
  j.key("shoulders").begin_object();
  for (int s = 0; s < 2; ++s) {
    j.key(s == 0 ? "r" : "l").begin_array();
    for (double v : se::kShoulderM[s]) j.num(v);
    j.end_array();
  }
  j.end_object();
  j.field("upper_arm", se::kUpperArmM).field("forearm", se::kForearmM);
  // The arm's ranges (viewmodel/arm.hpp): the builders' IK keeps every frame inside them.
  const ge::viewmodel::ArmLimits& l = ge::viewmodel::kHumanArm;
  j.key("limits").begin_object();
  j.field("shoulder_elevation_max", l.shoulder_elevation_max);
  j.field("shoulder_yaw_min", l.shoulder_yaw_min).field("shoulder_yaw_max", l.shoulder_yaw_max);
  j.field("elbow_flex_max", l.elbow_flex_max);
  j.field("elbow_roll_min", l.elbow_roll_min).field("elbow_roll_max", l.elbow_roll_max);
  j.field("wrist_extension_max", l.wrist_extension_max);
  j.field("wrist_flexion_max", l.wrist_flexion_max);
  j.field("wrist_radial_max", l.wrist_radial_max).field("wrist_ulnar_max", l.wrist_ulnar_max);
  j.field("forearm_twist_max", l.forearm_twist_max);
  j.end_object();
  j.key("weapons").begin_object();
  for (const se::WeaponLook& look : se::weapon_looks()) {
    j.key(look.name).begin_object();
    j.key("offset").begin_array();
    for (double v : look.offset_m) j.num(v);
    j.end_array();
    j.end_object();
  }
  j.end_object();
  j.end_object();
  return j.text();
}

const ge::assets::DataSpec kSpec{
    "se_view", "the held weapons' scale, shoulders, arm and offsets (view/weapons.hpp)", make};
GE_ASSET_DATA(kSpec);

}  // namespace
