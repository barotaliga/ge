// The skier's skeleton as a table, and the skis' outline.
//
// ge:summary The skier rig's joint table and the ski's half width along its length
// ge:keywords skier rig joints table ski_half_width sidecut
// ge:skill skier assets
// ge:test tests/skier_rig_test.cpp
#include "skier/app/rig.hpp"

namespace ge::skier {

namespace {

constexpr double kTip = kMountBack + kReferenceSkis.length / 2;
constexpr double kTail = kMountBack - kReferenceSkis.length / 2;

// Left and right mirror in x. The arms hang beside the body, the poles below the fists.
constexpr std::array<RigJoint, kRigJoints> kJoints = {{
    {"root", -1, {0, 0, 0}, 0},
    {"pelvis", 0, {0, 1.04, 0}, 0.15},
    {"spine", 1, {0, 1.14, 0}, 0.15},
    {"chest", 2, {0, 1.34, 0}, 0.17},
    {"neck", 3, {0, 1.52, 0}, 0.06},
    {"head", 4, {0, 1.64, 0}, 0.12},
    {"shoulder_l", 3, {-0.19, 1.47, 0}, 0.055},
    {"shoulder_r", 3, {0.19, 1.47, 0}, 0.055},
    {"elbow_l", 6, {-0.21, 1.19, 0}, 0.045},
    {"elbow_r", 7, {0.21, 1.19, 0}, 0.045},
    {"wrist_l", 8, {-0.22, 0.94, 0}, 0.045},
    {"wrist_r", 9, {0.22, 0.94, 0}, 0.045},
    {"pole_l", 10, {-0.22, 0.88, 0}, 0.009},
    {"pole_r", 11, {0.22, 0.88, 0}, 0.009},
    {"pole_tip_l", 12, {-0.22, 0.88 - kPoleLength, 0}, 0},
    {"pole_tip_r", 13, {0.22, 0.88 - kPoleLength, 0}, 0},
    {"hip_l", 1, {-0.11, 1.04, 0}, 0.075},
    {"hip_r", 1, {0.11, 1.04, 0}, 0.075},
    {"knee_l", 16, {-0.11, 0.60, 0}, 0.06},
    {"knee_r", 17, {0.11, 0.60, 0}, 0.06},
    {"ankle_l", 18, {-0.11, 0.17, 0}, 0.06},
    {"ankle_r", 19, {0.11, 0.17, 0}, 0.06},
    {"ski_tip_l", 20, {-0.11, 0, kTip}, 0},
    {"ski_tip_r", 21, {0.11, 0, kTip}, 0},
    {"ski_tail_l", 20, {-0.11, 0, kTail}, 0},
    {"ski_tail_r", 21, {0.11, 0, kTail}, 0},
}};

}  // namespace

const std::array<RigJoint, kRigJoints>& rig_joints() { return kJoints; }

int rig_joint(std::string_view name) {
  for (size_t i = 0; i < kJoints.size(); i++) {
    if (name == kJoints[i].name) return static_cast<int>(i);
  }
  return -1;
}

double ski_half_width(const SkiSpec& spec, double z) {
  const double from_waist = z - kMountBack;
  return spec.waist / 2 + from_waist * from_waist / (2 * spec.sidecut);
}

}  // namespace ge::skier
