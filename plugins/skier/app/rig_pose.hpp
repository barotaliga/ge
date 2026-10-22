// THE SKIER POSED: the rig's joints (rig.hpp) placed every frame from a `SkierState`, and the
// skier model's joint transforms made from them, so the skinned figure stands where the physics
// says and moves without the faults the placeholder figure showed (the anim lab, `--figure`).
//
// THE POSE, in the figure's frame (figure.hpp's FigureFrame: the snow at y = 0):
// - THE SKIS roll by the edge about the edge that is on the snow (the inside one in a turn), so a
//   ski tilts up off the snow and never into it; the boots ride them.
// - THE BODY inclines into the turn by the lean about the skis' base, the legs further than the
//   torso and the torso further than the head (the angulation that keeps the eyes level); the
//   tuck folds the hips and the torso. The lean the pose uses is the state's passed through two
//   lags in turn (`kLeanLag` each), so its RATE has no step either: one lag leaves the speed of
//   the head jumping at each change of turn, which is a pop.
// - THE LEGS reach from the hips to the ankles by two-bone IK, the knees forward; the pelvis is
//   lowered when a leg cannot reach, so a bone never stretches.
// - THE ARMS reach for the hands' places by two-bone IK, the elbows out and back.
// - THE POLES hang from the fists at their own length, the tips trailing back and out at a fixed
//   height over the snow, so a tip never goes through it.
// - DOWN (wiped out) the body and the skis lie over on their side, eased there by the same lags.
//
// THE MODEL: `RigBinding` finds every rig joint in a loaded skier model by name; `rig_locals`
// turns a pose into the model's local transforms (rotation from bind, the bind's own bone
// orientation kept), ready for `assets::pose_globals` and the GPU's skinning.
//
// ge:summary The skier's rig posed from a SkierState, and the skier model's locals from the pose
// ge:keywords skier rig pose poser ik two-bone lean angulation edge pivot pole tuck smoothing
// ge:keywords rig_locals rig-binding skinned figure model anim
// ge:skill skier assets
// ge:test tests/skier_rig_pose_test.cpp
#pragma once

#include <array>
#include <span>
#include <vector>

#include "assets/model.hpp"
#include "core/quat.hpp"
#include "skier/app/rig.hpp"
#include "skier/skier.hpp"

namespace ge::skier {

inline constexpr double kLeanLag = 0.07;  // s, each of the lean's two lags
inline constexpr double kEdgeLag = 0.03;  // s, the edge's one lag
inline constexpr double kTuckLag = 0.08;  // s

struct RigPose {
  std::array<core::Vec3, kRigJoints> at;    // every joint, in the figure's frame, m
  std::array<core::Quat, kRigJoints> turn;  // every joint's rotation from its bind
};

// Holds the lags between frames; one a skier.
class RigPoser {
 public:
  // The pose for `s` after `dt` seconds since the last one (the first call starts settled).
  void pose(const SkierState& s, double dt, RigPose& out);

 private:
  bool primed_ = false;
  double lean1_ = 0, lean2_ = 0, edge_ = 0, tuck_ = 0;
};

// Which model joint each rig joint is, and the model's bind, read once at load.
struct RigBinding {
  std::vector<int> rig_of;           // per model joint: its rig joint, or -1
  std::vector<assets::Affine> bind;  // per model joint: its bind global (model space)
  bool complete = false;             // every rig joint found
};
RigBinding bind_rig(const assets::Model& model);

// The model's locals for `pose` (sized to the model's joints).
void rig_locals(const assets::Model& model, const RigBinding& binding, const RigPose& pose,
                std::span<assets::Trs> local);

}  // namespace ge::skier
