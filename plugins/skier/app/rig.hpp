// THE SKIER'S RIG: the skeleton every skier model is built on and posed by, stated once. The
// model's builder (plugins/skier/assets/skier/build.py) is fed it as data (`skier` in
// assets/skier/data.cpp) and makes its bones from it; the poser reads the same joints back by
// name, so a bone moved here moves the model and the pose together, and the model goes stale
// until it is rebuilt (`ge asset build skier`).
//
// THE BIND POSE stands the skier tall on his skis on level snow, in the figure's frame
// (figure.hpp's FigureFrame): x his right, y up, z forward, the skis' base at y = 0 under the
// boots. The arms hang straight down beside the body and the poles straight down from the fists,
// so every limb's bone runs along -y at bind (its rest rotation the identity), which is what a
// builder lofts a limb around. A joint's `radius` is the limb's from it to its first child, for
// the builder's meshes and a lab's bone boxes.
//
// THE SKIS are the reference pair's (`kReferenceSkis`), which places the tip and tail joints.
// A ski's outline is half the waist plus z^2 / 2R along its length (z from the waist, R the
// sidecut); the boot is mounted `kMountBack` behind the ski's middle; the tip and the tail turn
// up over `kRockerLength`.
//
// ge:summary The skier's skeleton stated once: joints, bind positions, limb radii, the skis' cut
// ge:keywords skier rig skeleton joints bones bind-pose skier_rig rig-joint radius ski-outline
// ge:keywords sidecut mount rocker pole-length model builder
// ge:skill skier assets
// ge:test tests/skier_rig_test.cpp
#pragma once

#include <array>
#include <string_view>

#include "core/quat.hpp"
#include "skier/skis.hpp"

namespace ge::skier {

struct RigJoint {
  const char* name;
  int parent;     // -1 for the root
  core::Vec3 at;  // bind position in the figure's frame, m
  double radius;  // the limb's from here to its first child, m (0: no limb)
};

inline constexpr int kRigJoints = 26;
const std::array<RigJoint, kRigJoints>& rig_joints();
// The index of the joint of that name, or -1.
int rig_joint(std::string_view name);

inline constexpr double kMountBack = 0.08;     // the boot behind the ski's middle, m
inline constexpr double kSkiThick = 0.018;     // m
inline constexpr double kRockerLength = 0.16;  // m of the tip and the tail turned up
inline constexpr double kRockerRise = 0.045;   // m the very tip stands off the snow
inline constexpr double kPoleLength = 1.18;    // grip to tip, m

// A ski's half width at `z` metres along it from the boot (+ toward the tip).
double ski_half_width(const SkiSpec& spec, double z);

}  // namespace ge::skier
