// The arm holding a view model (engine/viewmodel/arm.hpp): a natural hold is within every range,
// an elbow pointing up, a wrist bent back past its range and an upper arm across the chest are
// each named, and a left arm mirrored from a right one reads the same angles.
//
// ge:summary Tests for the arm's joint angles and ranges: natural, elbow up, wrist, mirror
// ge:keywords test viewmodel arm elbow wrist shoulder range-of-motion faults mirror
// ge:skill render testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "viewmodel/arm.hpp"

namespace vm = ge::viewmodel;

namespace {

// A right arm in the view's frame (x right, y up, z forward): the shoulder below and right of the
// eye, the elbow down and out, the hand forward holding a pistol grip, thumb up.
vm::ArmPose natural() {
  vm::ArmPose p{};
  const double s[3] = {0.19, -0.24, -0.06}, e[3] = {0.27, -0.50, 0.08}, w[3] = {0.16, -0.40, 0.30};
  for (int k = 0; k < 3; ++k) {
    p.shoulder[k] = s[k];
    p.elbow[k] = e[k];
    p.wrist[k] = w[k];
    p.distal[k] = w[k] - e[k];  // the hand straight on from the forearm
  }
  p.radial[0] = 0, p.radial[1] = 1, p.radial[2] = 0;
  p.right[0] = 1, p.right[1] = 0, p.right[2] = 0;
  p.up[0] = 0, p.up[1] = 1, p.up[2] = 0;
  p.forward[0] = 0, p.forward[1] = 0, p.forward[2] = 1;
  p.side = 1;
  return p;
}

bool names(const std::vector<std::string>& faults, const char* what) {
  for (const std::string& f : faults)
    if (f.find(what) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(a_natural_hold_is_within_every_range) {
  const vm::ArmAngles a = vm::arm_angles(natural());
  CHECK(a.elbow_flex > 30 && a.elbow_flex < 145);
  CHECK(a.elbow_roll > 0 && a.elbow_roll < 90);  // down and out
  CHECK(std::fabs(a.wrist_extension) < 1 && std::fabs(a.wrist_radial) < 1);
  CHECK(std::fabs(a.forearm_twist) < 30);
  CHECK(a.shoulder_elevation < 0);
  CHECK(vm::arm_faults(a).empty());
  CHECK(vm::arm_excess(a) == 0);
}

TEST(an_elbow_pointing_up_is_named) {
  vm::ArmPose p = natural();
  p.elbow[0] = 0.20, p.elbow[1] = 0.02, p.elbow[2] = 0.12;  // above the line from shoulder to wrist
  for (int k = 0; k < 3; ++k) p.distal[k] = p.wrist[k] - p.elbow[k];
  const vm::ArmAngles a = vm::arm_angles(p);
  CHECK(std::fabs(a.elbow_roll) > 120);
  const std::vector<std::string> f = vm::arm_faults(a);
  CHECK(names(f, "elbow's point"));
  CHECK(names(f, "shoulder's elevation"));  // the upper arm raised over the shoulder
  CHECK(vm::arm_excess(a) > 0);
}

TEST(a_wrist_bent_back_past_its_range_is_named) {
  vm::ArmPose p = natural();
  // The hand turned 80 deg toward its back: for a right hand thumb up, the back faces right.
  const double f[3] = {p.wrist[0] - p.elbow[0], p.wrist[1] - p.elbow[1], p.wrist[2] - p.elbow[2]};
  const double l = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  // back = radial x forearm = up x f
  const double back[3] = {f[2] / l, 0, -f[0] / l};
  const double c = std::cos(80 * 3.141592653589793 / 180),
               s = std::sin(80 * 3.141592653589793 / 180);
  for (int k = 0; k < 3; ++k) p.distal[k] = f[k] / l * c + back[k] * s;
  const vm::ArmAngles a = vm::arm_angles(p);
  CHECK(std::fabs(a.wrist_extension - 80) < 3);
  CHECK(names(vm::arm_faults(a), "wrist's extension"));
}

TEST(an_upper_arm_across_the_chest_is_named) {
  vm::ArmPose p = natural();
  p.elbow[0] = -0.05, p.elbow[1] = -0.28, p.elbow[2] = 0.05;  // swung in across the body
  p.wrist[0] = -0.10, p.wrist[1] = -0.30, p.wrist[2] = 0.30;
  for (int k = 0; k < 3; ++k) p.distal[k] = p.wrist[k] - p.elbow[k];
  const vm::ArmAngles a = vm::arm_angles(p);
  CHECK(a.shoulder_yaw < -45);
  CHECK(names(vm::arm_faults(a), "shoulder's swing"));
}

TEST(a_left_arm_mirrored_reads_the_same) {
  const vm::ArmPose r = natural();
  vm::ArmPose l = r;
  for (double* v : {l.shoulder, l.elbow, l.wrist, l.distal, l.radial}) v[0] = -v[0];
  l.side = -1;
  const vm::ArmAngles a = vm::arm_angles(r), b = vm::arm_angles(l);
  CHECK(std::fabs(a.elbow_roll - b.elbow_roll) < 1e-9);
  CHECK(std::fabs(a.elbow_flex - b.elbow_flex) < 1e-9);
  CHECK(std::fabs(a.shoulder_yaw - b.shoulder_yaw) < 1e-9);
  CHECK(std::fabs(a.wrist_extension - b.wrist_extension) < 1e-9);
  CHECK(std::fabs(a.wrist_radial - b.wrist_radial) < 1e-9);
  CHECK(std::fabs(std::fabs(a.forearm_twist) - std::fabs(b.forearm_twist)) < 1e-9);
}
