// A SKELETAL MODEL BAKED INTO A MORPH MODEL: an authored, skinned model (assets/model.hpp; a
// mod's `.iqm`, assets/iqm.hpp) posed on the CPU at a steady rate through each of its clips, every
// pose a frame, so a game that draws vertex-animated models (the morph pass, render/morph.hpp)
// draws a skeletal one with the same lighting, tints, warps and frame blending, and nothing new on
// the GPU.
//
// - THE FRAMES: clip c is frame group c (an entity's `frame` names it), its poses `fps` apart from
//   its start to its end, the last left off when it is the first again (a looping clip); a group
//   of more than one plays them at 1 / fps each. A model with no clips is one frame at rest.
// - THE FRAME: the engine's (+y up, front +z, metres) turned into the classic file's (x front,
//   y left, z up) and divided by `metres`, so the model is in the units a classic model is, and
//   each triangle's last two corners swap to keep its outside, the frame's mirror undone.
// - THE SKIN: every mesh's texture coordinates moved into its material's rect of one atlas
//   (`rects`, a rect a material: u0, v0, u1, v1; none, the coordinates as they are), so a game
//   lays the materials' pictures out in one picture and draws it as the model's one skin. The
//   skin itself is the caller's: the model comes back with one 8 by 8 skin of palette entry
//   `grey`, for a game that finds no picture.
//
// A FAULT (more than `kMaxBakedFrames` frames, a mesh with nothing in it) names the model, and
// nothing is made.
//
// ge:summary A skeletal model posed through its clips into a morph model's frames, one skin atlas
// ge:keywords morph skeletal bake_skeletal iqm frame-group clip frames atlas uv-rect cpu-skinning
// ge:keywords mod-model skeletal-model
// ge:skill morph assets
// ge:test tests/morph_skeletal_test.cpp
#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>

#include "assets/model.hpp"
#include "morph/model.hpp"

namespace ge::morph {

inline constexpr int kMaxBakedFrames = 4096;

struct BakeOptions {
  double fps = 20;    // poses a second through a clip
  double metres = 1;  // one of the model's units, m (the `metres` it was read at)
  uint8_t grey = 8;   // the stand-in skin's palette entry
  std::span<const std::array<float, 4>> rects = {};  // a material's place in the atlas, 0..1
};

bool bake_skeletal(const assets::Model& in, std::string_view name, const BakeOptions& options,
                   Model* out, std::string* error);

}  // namespace ge::morph
