// THE ARM THAT HOLDS A VIEW MODEL: a human arm's three joints and what each can do, so a
// first-person model's hands and arms (a built weapon's `hand_<side>` nodes) are held to anatomy
// rather than to whatever an IK solve found. One authority: the builders' IK picks its elbow by
// these ranges (fed to it as data) and the labs and tests measure every frame by them.
//
// - THE POSE (`ArmPose`) is points and directions in any frame whose `right`, `up` and `forward` it
//   names (the view's, or a model's turned one): the shoulder, the elbow, the wrist, the hand's
//   DISTAL direction (wrist to knuckles) and its RADIAL one (toward the thumb and index side), and
//   the body's own forward and up at the shoulder (the torso turns them).
// - THE ANGLES (`arm_angles`), degrees:
//     shoulder_elevation  the upper arm above (+) or below the shoulder's horizontal
//     shoulder_yaw        the upper arm round from straight forward: +90 out to its side, -90
//     across
//                         the chest, 180 behind (undefined when it hangs within 30 deg of straight
//                         down: 0)
//     elbow_flex          0 straight, the hinge's bend
//     elbow_roll          where the elbow points off the shoulder-to-wrist line: 0 straight down,
//                         +90 out to its side, -90 in, 180 up (undefined below 10 deg of flex: 0)
//     wrist_extension     the hand bent toward its back (+) or its palm (-) off the forearm's line
//     wrist_radial        the hand bent toward the thumb's side (+) or the little finger's (-)
//     forearm_twist       the turn of the hand's radial side about the forearm from thumb-up (0):
//                         pronation and supination alike, its size (undefined below 10 deg of flex)
// - THE RANGES (`ArmLimits`, `kHumanArm`): what a person holding something does, a little short of
//   what a joint can do at most: the elbow never points up or in across the body, it bends 0 to
//   145, the wrist extends 70 and flexes 75, deviates 20 toward the thumb and 35 away, the forearm
//   twists 95 either way, and the upper arm rises at most 25 above the shoulder and swings from 45
//   across the chest to 135 out and back. `arm_faults` names each joint outside its range.
//
// ge:summary The arm holding a view model: its joints' angles and their human ranges, the faults
// ge:keywords viewmodel arm shoulder elbow wrist joint range-of-motion anatomy ik limits hands
// ge:keywords arm_angles arm_faults arm-pose elbow-roll wrist-extension radial-deviation pronation
// ge:keywords supination forearm-twist khumanarm arm_poses hand-nodes torso
// ge:skill render held-weapons
// ge:test tests/viewmodel_arm_test.cpp
#pragma once

#include <span>
#include <string>
#include <vector>

#include "assets/model.hpp"

namespace ge::viewmodel {

struct ArmPose {
  double shoulder[3], elbow[3], wrist[3];
  double distal[3];  // the hand: wrist to knuckles (any length)
  double radial[3];  // the hand: toward the thumb and index side (any length)
  double right[3];   // the frame's (or the turned body's) right, up and forward at the shoulder
  double up[3];
  double forward[3];
  int side = 1;  // +1 the right arm, -1 the left
};

struct ArmAngles {
  double shoulder_elevation = 0, shoulder_yaw = 0;
  double elbow_flex = 0, elbow_roll = 0;
  double wrist_extension = 0, wrist_radial = 0;
  double forearm_twist = 0;
};

struct ArmLimits {
  double shoulder_elevation_max = 25;
  double shoulder_yaw_min = -45, shoulder_yaw_max = 135;
  double elbow_flex_max = 145;
  double elbow_roll_min = -30, elbow_roll_max = 100;
  double wrist_extension_max = 70, wrist_flexion_max = 75;
  double wrist_radial_max = 20, wrist_ulnar_max = 35;
  double forearm_twist_max = 95;
};
inline constexpr ArmLimits kHumanArm{};

ArmAngles arm_angles(const ArmPose& pose);

// Every joint of `a` outside `limits`, each a sentence naming the joint, its angle and its range.
std::vector<std::string> arm_faults(const ArmAngles& a, const ArmLimits& limits = kHumanArm);

// How far outside its ranges `a` is in all, degrees summed (0: within them): what an IK search
// drives to nothing.
double arm_excess(const ArmAngles& a, const ArmLimits& limits = kHumanArm);

// The arms of a model built round its hands (the held-weapons kit's nodes: `<name>.hand_<s>_upper`
// at the shoulder, `<name>.hand_<s>_fore` at the elbow, `<name>.hand_<s>` at the wrist, built with
// +y distal and +x radial, which arrive through glTF's y-up and the loader's mirror as the node's
// -z and -x; the body's stance and turn on `<name>.torso`),
// posed by `global` (assets::pose_globals), in the model's frame whose right, up and forward are
// given. Fills `out` with up to two (the right first) and returns how many it found.
int arm_poses(const assets::Model& model, std::span<const assets::Affine> global,
              const double right[3], const double up[3], const double forward[3], ArmPose out[2]);

}  // namespace ge::viewmodel
