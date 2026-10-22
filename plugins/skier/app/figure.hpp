// THE SKIER AS A FIGURE, a placeholder until the skinned figure (docs/goals.md M8): rigid pieces
// (the skis, the boots, the legs, the pelvis, the torso, the arms, the head, the poles), each a
// box, a cylinder or a ball of its own size, posed every frame from a `SkierState`. Any skiing
// game draws a racer by placing one instance a piece (render/instances.hpp: a unit shape scaled
// to the piece's size); the pieces and their parts are its whole contract.
//
// THE POSE, from what the state says: the skis lie along the snow under the boots (the ground's
// normal on the snow, the flight's pitch in the air) rolled by `edge`; the body stands on them
// rolled by `lean` into the turn; `tuck` folds the knees, the hips and the torso (a hip 0.99 m
// over the snow standing, 0.65 m in a full tuck) and brings the hands in front of the knees; the
// poles trail back from the hands. Down (wiped out) the figure lies on its side on the snow.
//
// THE BODY FRAME of a piece's pose: x the skier's right, y up, z forward (engine/core/quat.hpp).
// A pose is the piece's centre in the world, its rotation (body to world) and its size, the unit
// shape's scale (a box and a ball are 1 m across, a cylinder 1 m across and 1 m tall along y).
//
// ge:summary The skier as rigid pieces posed from a SkierState: the placeholder figure
// ge:keywords skier figure placeholder pose pieces skis boots legs torso head poles tuck lean
// ge:keywords edge body-frame figure_pieces pose_figure figure-part figure_frame figure_joints
// ge:keywords joint rig anim
// ge:skill skier render
// ge:test tests/skier_figure_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "core/quat.hpp"
#include "skier/skier.hpp"

namespace ge::skier {

enum class FigureShape : uint8_t { box, cylinder, ball };

// What a piece is made of, for a game's materials.
enum class FigurePart : uint8_t { ski, boot, legs, jacket, helmet, pole };
inline constexpr int kFigureParts = 6;

struct FigurePiece {
  FigureShape shape;
  FigurePart part;
};

inline constexpr int kFigurePieces = 17;

// The pieces, in the order pose_figure poses them.
const std::array<FigurePiece, kFigurePieces>& figure_pieces();

struct PiecePose {
  core::Vec3 centre;  // world, m
  core::Quat rotation;
  double size[3];  // the unit shape's scale, m: x across, y up its length, z along
};

// Pose every piece for `s` on `world`'s snow.
void pose_figure(const SkierState& s, const World& world,
                 std::array<PiecePose, kFigurePieces>& out);

// THE FIGURE'S FRAME: the skis' base under the boots, turned by the heading and the snow (or the
// flight's pitch) but not by the edge or the lean, so the snow is y = 0 in it: the frame an
// animation of the figure is recorded and judged in (engine/lab/anim.hpp).
struct FigureFrame {
  core::Vec3 at;   // world, m
  core::Quat rot;  // the frame to the world
};
FigureFrame figure_frame(const SkierState& s, const World& world);

// THE JOINTS the posed pieces meet at, for an animation's pose track: each with the joint its
// bone starts at (-1 for the waist, the root) and its side (-1 left, 0 centre, 1 right).
struct FigureJoint {
  const char* name;
  int parent;
  int side;
};
inline constexpr int kFigureJoints = 23;
const std::array<FigureJoint, kFigureJoints>& figure_joints();

// Every joint of `pieces` (posed by pose_figure) in `frame`, m.
void figure_joint_positions(const std::array<PiecePose, kFigurePieces>& pieces,
                            const FigureFrame& frame, std::array<core::Vec3, kFigureJoints>& out);

// The pelvis's height over the skis' base, m, for a tuck of 0..1 (standing 0.99, folded 0.65).
double figure_hip_height(double tuck);

}  // namespace ge::skier
