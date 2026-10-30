// A MORPH MODEL WRITTEN AS glTF: a `morph::Model` out as one binary glTF 2.0 file (.glb) any
// glTF reader can load, so a game reads the cache's models the way it reads every other model.
//
// - **The mesh** is the first frame's positions and smooth normals (`frame_normals`), the
//   texture coordinates, and the triangles. **Every frame is a morph target** (its positions
//   and normals as differences from the first frame's), in the file's order, so weight `k` at 1
//   and the rest at 0 shows frame `k`; target 0 is all zeros. The target names (the frames'
//   names) are in the mesh's `extras.targetNames`.
// - **The clips** are animations of the morph weights with STEP sampling: a frame group plays
//   its frames at their own intervals; a run of lone frames whose names share a stem (`walk1`,
//   `walk2` ...) plays at `frame_seconds` each. A clip is named after its stem.
// - **The skins** are the caller's encoded pictures (PNG), embedded: skin 0 is the mesh's
//   material; the others are materials of their own, named `skin1`, `skin2` ..., for a game
//   that swaps them. A skin's optional glow picture is its material's emissive texture.
// - **The frame**: the classic file's x forward, y left, z up becomes glTF's z forward, x left,
//   y up (a rotation, so the triangles keep their outside), scaled by `unit_m` into metres.
//
// ge:summary A morph model written as a binary glTF: morph targets, weight clips, embedded skins
// ge:keywords morph gltf glb write morph-targets weights clip step skin emissive export
// ge:skill morph
// ge:test tests/morph_glb_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "morph/model.hpp"

namespace ge::morph {

struct GlbSkin {
  std::vector<uint8_t> png;       // the skin's colour, an encoded PNG
  std::vector<uint8_t> glow_png;  // its emissive picture, an encoded PNG; empty: none
};

struct GlbOptions {
  float unit_m = 1.0f;         // metres a model unit
  float frame_seconds = 0.1f;  // how long a lone frame shows in a clip
};

// The model as a .glb. `name` names the mesh and its node. `skins` may be empty (no material).
std::vector<uint8_t> write_glb(const Model& m, const std::string& name,
                               const std::vector<GlbSkin>& skins, const GlbOptions& opts = {});

// The clip a frame belongs to, by the rule above: one name a clip, in the frames' order, and
// for every frame its clip's index.
struct Clips {
  std::vector<std::string> names;
  std::vector<int> of_frame;
};
Clips clips_of(const Model& m);

}  // namespace ge::morph
