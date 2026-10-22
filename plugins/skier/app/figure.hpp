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
// ge:keywords edge body-frame figure_pieces pose_figure figure-part
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

// The pelvis's height over the skis' base, m, for a tuck of 0..1 (standing 0.99, folded 0.65).
double figure_hip_height(double tuck);

}  // namespace ge::skier
