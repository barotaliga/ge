// The body's catalog (anatomy.hpp): the parts' names and sides, and the injuries' table.
//
// ge:summary The body's catalog: the parts' names and sides and every injury's row
// ge:keywords anatomy injuries catalog injury_def find_injury part_name base_of sided
// ge:skill skier
// ge:test tests/skier_body_test.cpp
#include "skier/anatomy.hpp"

#include <array>

namespace ge::skier {

namespace {

constexpr uint32_t bit(Bone b) { return 1u << static_cast<uint32_t>(b); }
constexpr uint16_t bit(Organ o) { return static_cast<uint16_t>(1u << static_cast<uint32_t>(o)); }

constexpr std::array<const char*, kParts> kPartNames = {
    "head",       "neck",   "chest",  "back",   "abdomen", "pelvis",  "shoulder_l",
    "shoulder_r", "arm_l",  "arm_r",  "hand_l", "hand_r",  "thigh_l", "thigh_r",
    "knee_l",     "knee_r", "shin_l", "shin_r", "foot_l",  "foot_r"};

// Every injury: name, part, AIS, mechanism, even-chance dose, the side an organ is hurt from,
// the sides a bone breaks from, the fracture, its bones, its organs, said. Within a part the
// order is free: `body.cpp` sorts each part's ladder worst first.
constexpr InjuryDef kInjuries[] = {
    {"head_bump", Base::head, 1, Mech::blunt, 45, Facing::none, 0, Fracture::none, 0, 0, false},
    {"concussion", Base::head, 2, Mech::blunt, 95, Facing::none, 0, Fracture::none, 0,
     bit(Organ::brain), false},
    {"knocked_out", Base::head, 3, Mech::blunt, 150, Facing::none, 0, Fracture::none, 0,
     bit(Organ::brain), false},
    {"skull_fracture", Base::head, 4, Mech::blunt, 230, Facing::none, 0, Fracture::broken,
     bit(Bone::skull), 0, false},
    {"cracked_skull", Base::head, 2, Mech::blunt, 184, Facing::none, 0, Fracture::hairline,
     bit(Bone::skull), 0, false},
    {"broken_jaw", Base::head, 2, Mech::blunt, 125, Facing::front, 0, Fracture::broken,
     bit(Bone::mandible), 0, false},
    {"cracked_jaw", Base::head, 1, Mech::blunt, 100, Facing::front, 0, Fracture::hairline,
     bit(Bone::mandible), 0, false},
    {"brain_injury", Base::head, 5, Mech::blunt, 320, Facing::none, 0, Fracture::none, 0,
     bit(Organ::brain), false},
    {"whiplash", Base::neck, 1, Mech::blunt, 24, Facing::none, 0, Fracture::none, 0, 0, false},
    {"neck_sprain", Base::neck, 2, Mech::blunt, 45, Facing::none, 0, Fracture::none, 0, 0, false},
    {"neck_fracture", Base::neck, 3, Mech::blunt, 80, Facing::none, 0, Fracture::hairline,
     bit(Bone::cervical), 0, false},
    {"broken_neck", Base::neck, 5, Mech::blunt, 130, Facing::none, 0, Fracture::broken,
     bit(Bone::cervical), 0, false},
    {"bruised_ribs", Base::chest, 1, Mech::blunt, 24, Facing::none, 0, Fracture::none, 0, 0, false},
    {"cracked_ribs", Base::chest, 1, Mech::blunt, 42, Facing::none, 0, Fracture::hairline,
     bit(Bone::ribs), 0, false},
    {"broken_ribs", Base::chest, 2, Mech::blunt, 52, Facing::none, 0, Fracture::broken,
     bit(Bone::ribs), 0, false},
    {"cracked_sternum", Base::chest, 1, Mech::blunt, 58, Facing::front, 0, Fracture::hairline,
     bit(Bone::sternum), 0, false},
    {"broken_sternum", Base::chest, 2, Mech::blunt, 72, Facing::front, 0, Fracture::broken,
     bit(Bone::sternum), 0, false},
    {"collapsed_lung", Base::chest, 3, Mech::blunt, 85, Facing::none, 0, Fracture::none, 0,
     bit(Organ::lung), false},
    {"flail_chest", Base::chest, 4, Mech::blunt, 130, Facing::none, 0, Fracture::broken,
     bit(Bone::ribs) | bit(Bone::sternum), 0, false},
    {"crushed_chest", Base::chest, 5, Mech::blunt, 340, Facing::none, 0, Fracture::broken,
     bit(Bone::ribs) | bit(Bone::sternum), bit(Organ::lung), false},
    {"torn_aorta", Base::chest, 5, Mech::blunt, 300, Facing::front, 0, Fracture::none, 0,
     bit(Organ::heart), false},
    {"bruised_back", Base::back, 1, Mech::blunt, 24, Facing::none, 0, Fracture::none, 0, 0, false},
    {"back_strain", Base::back, 1, Mech::load, 13, Facing::none, 0, Fracture::none, 0, 0, false},
    {"compressed_vertebra", Base::back, 2, Mech::load, 15.5, Facing::none, 0, Fracture::hairline,
     bit(Bone::lumbar), 0, false},
    {"broken_back", Base::back, 3, Mech::load, 21, Facing::none, 0, Fracture::broken,
     bit(Bone::lumbar), 0, false},
    {"broken_back_blow", Base::back, 3, Mech::blunt, 75, Facing::none, 0, Fracture::broken,
     bit(Bone::thoracic), 0, false},
    {"cracked_vertebra", Base::back, 1, Mech::blunt, 60, Facing::none, 0, Fracture::hairline,
     bit(Bone::thoracic), 0, false},
    {"spinal_cord", Base::back, 5, Mech::load, 30, Facing::none, 0, Fracture::broken,
     bit(Bone::lumbar), 0, true},
    {"winded", Base::abdomen, 1, Mech::blunt, 20, Facing::none, 0, Fracture::none, 0, 0, false},
    {"bruised_kidney", Base::abdomen, 2, Mech::blunt, 42, Facing::back, 0, Fracture::none, 0,
     bit(Organ::kidney), false},
    {"torn_kidney", Base::abdomen, 3, Mech::blunt, 75, Facing::back, 0, Fracture::none, 0,
     bit(Organ::kidney), false},
    {"torn_spleen", Base::abdomen, 3, Mech::blunt, 62, Facing::left, 0, Fracture::none, 0,
     bit(Organ::spleen), false},
    {"ruptured_spleen", Base::abdomen, 4, Mech::blunt, 100, Facing::left, 0, Fracture::none, 0,
     bit(Organ::spleen), false},
    {"torn_liver", Base::abdomen, 3, Mech::blunt, 62, Facing::right, 0, Fracture::none, 0,
     bit(Organ::liver), false},
    {"shattered_spleen", Base::abdomen, 5, Mech::blunt, 250, Facing::left, 0, Fracture::none, 0,
     bit(Organ::spleen), false},
    {"shattered_liver", Base::abdomen, 5, Mech::blunt, 250, Facing::right, 0, Fracture::none, 0,
     bit(Organ::liver), false},
    {"lacerated", Base::abdomen, 4, Mech::blunt, 105, Facing::front, 0, Fracture::none, 0,
     bit(Organ::stomach) | bit(Organ::bowel), false},
    {"bruised_hip", Base::pelvis, 1, Mech::blunt, 18, Facing::none, 0, Fracture::none, 0, 0, false},
    {"cracked_pelvis", Base::pelvis, 2, Mech::blunt, 48, Facing::none, 0, Fracture::hairline,
     bit(Bone::pelvis), 0, false},
    {"broken_pelvis", Base::pelvis, 3, Mech::blunt, 75, Facing::none, 0, Fracture::broken,
     bit(Bone::pelvis), 0, false},
    {"crushed_pelvis", Base::pelvis, 5, Mech::blunt, 380, Facing::none, 0, Fracture::broken,
     bit(Bone::pelvis), 0, false},
    {"bruised_shoulder", Base::shoulder, 1, Mech::blunt, 15, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"separated_shoulder", Base::shoulder, 1, Mech::blunt, 24, Facing::none, kOnOutside,
     Fracture::none, 0, 0, false},
    {"dislocated_shoulder", Base::shoulder, 2, Mech::blunt, 31, Facing::none, kOnOutside,
     Fracture::none, 0, 0, false},
    {"cracked_collarbone", Base::shoulder, 1, Mech::blunt, 30, Facing::none, kOnOutside,
     Fracture::hairline, bit(Bone::clavicle), 0, false},
    {"broken_collarbone", Base::shoulder, 2, Mech::blunt, 38, Facing::none, kOnOutside,
     Fracture::broken, bit(Bone::clavicle), 0, false},
    {"cracked_scapula", Base::shoulder, 1, Mech::blunt, 60, Facing::none, kOnBack,
     Fracture::hairline, bit(Bone::scapula), 0, false},
    {"broken_scapula", Base::shoulder, 2, Mech::blunt, 78, Facing::none, kOnBack, Fracture::broken,
     bit(Bone::scapula), 0, false},
    {"bruised_elbow", Base::arm, 1, Mech::blunt, 16, Facing::none, 0, Fracture::none, 0, 0, false},
    {"dislocated_elbow", Base::arm, 2, Mech::blunt, 36, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"cracked_arm", Base::arm, 1, Mech::blunt, 37, Facing::none, 0, Fracture::hairline,
     bit(Bone::humerus), 0, false},
    {"broken_arm", Base::arm, 2, Mech::blunt, 46, Facing::none, 0, Fracture::broken,
     bit(Bone::humerus), 0, false},
    {"cracked_forearm", Base::arm, 1, Mech::blunt, 33, Facing::none, 0, Fracture::hairline,
     bit(Bone::ulna), 0, false},
    {"broken_forearm", Base::arm, 2, Mech::blunt, 41, Facing::none, 0, Fracture::broken,
     bit(Bone::radius) | bit(Bone::ulna), 0, false},
    {"sprained_thumb", Base::hand, 1, Mech::blunt, 12, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"sprained_wrist", Base::hand, 1, Mech::blunt, 17, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"skiers_thumb", Base::hand, 2, Mech::blunt, 21, Facing::none, 0, Fracture::none, 0, 0, false},
    {"cracked_wrist", Base::hand, 1, Mech::blunt, 22, Facing::none, 0, Fracture::hairline,
     bit(Bone::hand), 0, false},
    {"cracked_radius", Base::hand, 1, Mech::blunt, 24, Facing::none, 0, Fracture::hairline,
     bit(Bone::radius), 0, false},
    {"broken_wrist", Base::hand, 2, Mech::blunt, 28, Facing::none, 0, Fracture::broken,
     bit(Bone::radius), 0, false},
    {"broken_hand", Base::hand, 2, Mech::blunt, 33, Facing::none, 0, Fracture::broken,
     bit(Bone::hand), 0, false},
    {"dead_leg", Base::thigh, 1, Mech::blunt, 26, Facing::none, 0, Fracture::none, 0, 0, false},
    {"cracked_femur", Base::thigh, 2, Mech::blunt, 72, Facing::none, 0, Fracture::hairline,
     bit(Bone::femur), 0, false},
    {"broken_femur", Base::thigh, 3, Mech::blunt, 90, Facing::none, 0, Fracture::broken,
     bit(Bone::femur), 0, false},
    {"sprained_knee", Base::knee, 1, Mech::drawer, 13, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"bruised_knee", Base::knee, 1, Mech::blunt, 20, Facing::none, 0, Fracture::none, 0, 0, false},
    {"torn_mcl", Base::knee, 2, Mech::twist, 9.5, Facing::none, 0, Fracture::none, 0, 0, false},
    {"torn_acl", Base::knee, 2, Mech::drawer, 15, Facing::none, 0, Fracture::none, 0, 0, false},
    {"torn_acl_twist", Base::knee, 2, Mech::twist, 11.5, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"torn_meniscus", Base::knee, 2, Mech::twist, 13, Facing::none, 0, Fracture::none, 0, 0, false},
    {"cracked_kneecap", Base::knee, 1, Mech::blunt, 48, Facing::none, kOnFront, Fracture::hairline,
     bit(Bone::patella), 0, false},
    {"broken_kneecap", Base::knee, 2, Mech::blunt, 60, Facing::none, kOnFront, Fracture::broken,
     bit(Bone::patella), 0, false},
    {"bruised_shin", Base::shin, 1, Mech::blunt, 22, Facing::none, 0, Fracture::none, 0, 0, false},
    {"cracked_boot_top", Base::shin, 1, Mech::bend, 16, Facing::none, 0, Fracture::hairline,
     bit(Bone::tibia), 0, false},
    {"boot_top", Base::shin, 2, Mech::bend, 20, Facing::none, 0, Fracture::broken,
     bit(Bone::tibia) | bit(Bone::fibula), 0, false},
    {"cracked_shin", Base::shin, 1, Mech::blunt, 76, Facing::none, 0, Fracture::hairline,
     bit(Bone::tibia), 0, false},
    {"broken_shin", Base::shin, 2, Mech::blunt, 95, Facing::none, 0, Fracture::broken,
     bit(Bone::tibia) | bit(Bone::fibula), 0, false},
    {"bruised_foot", Base::foot, 1, Mech::blunt, 28, Facing::none, 0, Fracture::none, 0, 0, false},
    {"sprained_ankle", Base::foot, 1, Mech::blunt, 45, Facing::none, 0, Fracture::none, 0, 0,
     false},
    {"cracked_ankle", Base::foot, 1, Mech::blunt, 56, Facing::none, 0, Fracture::hairline,
     bit(Bone::fibula), 0, false},
    {"broken_ankle", Base::foot, 2, Mech::blunt, 70, Facing::none, 0, Fracture::broken,
     bit(Bone::fibula), 0, false},
    {"cracked_foot", Base::foot, 1, Mech::blunt, 68, Facing::none, 0, Fracture::hairline,
     bit(Bone::foot), 0, false},
    {"broken_foot", Base::foot, 2, Mech::blunt, 85, Facing::none, 0, Fracture::broken,
     bit(Bone::foot), 0, false},
    {"cracked_heel", Base::foot, 1, Mech::load, 17, Facing::none, 0, Fracture::hairline,
     bit(Bone::foot), 0, false},
    {"broken_heel", Base::foot, 2, Mech::load, 21, Facing::none, 0, Fracture::broken,
     bit(Bone::foot), 0, false},
    {"pilon_fracture", Base::shin, 3, Mech::load, 25, Facing::none, 0, Fracture::broken,
     bit(Bone::tibia) | bit(Bone::fibula), 0, false},
    {"plateau_fracture", Base::knee, 3, Mech::load, 27, Facing::none, 0, Fracture::broken,
     bit(Bone::tibia), 0, false},
    {"broken_hip_socket", Base::pelvis, 3, Mech::load, 30, Facing::none, 0, Fracture::broken,
     bit(Bone::pelvis), 0, false},
    {"femur_driven", Base::thigh, 3, Mech::load, 33, Facing::none, 0, Fracture::broken,
     bit(Bone::femur), 0, false},
    {"brain_bleed", Base::head, 4, Mech::blunt, 265, Facing::none, 0, Fracture::none, 0,
     bit(Organ::brain), false},
    {"bruised_lung", Base::chest, 3, Mech::blunt, 70, Facing::none, 0, Fracture::none, 0,
     bit(Organ::lung), false},
    {"bruised_heart", Base::chest, 3, Mech::blunt, 110, Facing::front, 0, Fracture::none, 0,
     bit(Organ::heart), false},
    {"ruptured_liver", Base::abdomen, 4, Mech::blunt, 100, Facing::right, 0, Fracture::none, 0,
     bit(Organ::liver), false},
    {"bruised_bowel", Base::abdomen, 2, Mech::blunt, 70, Facing::front, 0, Fracture::none, 0,
     bit(Organ::bowel), false},
    {"torn_bladder", Base::pelvis, 3, Mech::blunt, 150, Facing::front, 0, Fracture::none, 0,
     bit(Organ::bladder), false},
    {"bruised_lung_fall", Base::chest, 3, Mech::load, 30, Facing::none, 0, Fracture::none, 0,
     bit(Organ::lung), false},
    {"torn_liver_fall", Base::abdomen, 3, Mech::load, 32, Facing::none, 0, Fracture::none, 0,
     bit(Organ::liver), false},
    {"torn_spleen_fall", Base::abdomen, 3, Mech::load, 34, Facing::none, 0, Fracture::none, 0,
     bit(Organ::spleen), false},
};

}  // namespace

Base base_of(Part p) {
  const auto i = static_cast<int32_t>(p);
  return i < 6 ? static_cast<Base>(i) : static_cast<Base>(6 + (i - 6) / 2);
}

int32_t side_of(Part p) {
  const auto i = static_cast<int32_t>(p);
  return i < 6 ? 0 : ((i - 6) % 2 == 0 ? -1 : 1);
}

Part sided(Base b, int32_t side) {
  const auto i = static_cast<int32_t>(b);
  return i < 6 ? static_cast<Part>(i) : static_cast<Part>(6 + (i - 6) * 2 + (side < 0 ? 0 : 1));
}

const char* part_name(Part p) { return kPartNames[static_cast<size_t>(p)]; }

std::span<const InjuryDef> injuries_catalog() { return kInjuries; }

int32_t find_injury(std::string_view name) {
  for (size_t k = 0; k < std::size(kInjuries); k++) {
    if (name == kInjuries[k].name) return static_cast<int32_t>(k);
  }
  return -1;
}

}  // namespace ge::skier
