// THE glTF LOADER: a glTF 2.0 file (.glb, or .gltf with its buffers embedded or beside it) read
// into a `Model` (assets/model.hpp) in the engine's frame, at load time. It is the one place a
// file format meets the engine: everything after it reads the Model.
//
// What it takes from the file:
// - every node of the scene as a joint, parents first, with its rest transform (TRS, or a
//   matrix split into one);
// - every mesh node's triangles as one Mesh (named after the node), its primitives' materials
//   as parts: POSITION, NORMAL (a mesh without normals is made faceted: each triangle its own
//   vertices and its face's normal), JOINTS_0 and WEIGHTS_0 (a skinned mesh), and the indices;
//   a mesh with no skin is a rigid part bound whole to its node's joint;
// - each skin's inverse bind matrices (a joint no skin names gets the inverse of its rest);
// - each material's base colour factor (linear), and its base colour texture BAKED INTO PARTS:
//   an embedded PNG (assets/image.hpp, palettes included) read at its TEXCOORD set (and its
//   KHR_texture_transform), each triangle taking the texel at its middle, linear, times the
//   factor, as a part of its own a colour (`<material>#rrggbb`), the vertices two colours share
//   split. A low-poly kit's colour atlas draws exactly, and every pass that draws parts draws
//   it; a photograph's texture is reduced to a colour a face, and a note says so;
// - or, when asked (`LoadOptions::keep_textures`), each base colour texture KEPT WHOLE: the
//   picture as one of the model's images, the material naming it, every mesh's texture
//   coordinates (the transform applied; a vertex of an untextured primitive at 0, 0), and the
//   material's emissive texture kept the same way as its glow; nothing is baked into parts. A
//   pass that draws pictures (a held weapon's skin, engine/viewmodel) asks for this;
// - each mesh's morph targets (their POSITION and NORMAL offsets, their names from the mesh's
//   `targetNames`, the mesh's or its node's default weights);
// - each animation as a Clip baked at kClipRateHz: translations, rotations and scales, with
//   STEP, LINEAR (rotations on the arc) and CUBICSPLINE samplers, and the morph weights as a
//   track for each mesh they move.
//
// THE FRAME. glTF is right-handed with +y up and an asset's front toward +z, so its +x is the
// asset's left; the engine's +x is its right. Every position, normal, rotation and matrix is
// mirrored in x on the way in (a rotation (x, y, z, w) becomes (x, -y, -z, w)), and every
// triangle's last two indices swap so its front stays outside.
//
// What it skips, each named in `notes` so a reader knows what the model lost: a texture it
// cannot read (a JPEG, a URI image) and every texture but the base colour's, a second set of
// joints and weights, a target's tangent offsets, cameras and lights. Points and lines are an
// error.
//
// ge:summary The glTF 2.0 loader: nodes, skins, meshes, materials and clips into a Model
// ge:keywords assets gltf glb cgltf loader import load_gltf load_gltf_file mirror handedness
// ge:keywords skin inverse-bind joints weights animation sampler step linear cubicspline
// ge:keywords slerp bake clip base-colour faceted rigid-node blender export
// ge:keywords morph-target target-names weights-channel shape-key blend-shape
// ge:keywords base-colour-texture texture-bake colour-atlas texcoord khr-texture-transform
// ge:keywords keep_textures load-options kept-texture emissive-texture glow uv
// ge:skill assets
// ge:test tests/assets_gltf_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/model.hpp"

namespace ge::assets {

struct LoadResult {
  bool ok = false;
  Model model;
  std::string error;               // when not ok: what failed, where, and the next action
  std::vector<std::string> notes;  // what was skipped, one line each
};

struct LoadOptions {
  bool keep_textures = false;  // keep the base colour and emissive pictures whole (above)
};

// A model from a file's bytes. `where` names it in messages and is the path relative URIs (a
// .gltf's .bin) resolve against.
LoadResult load_gltf(std::span<const uint8_t> bytes, std::string_view where,
                     const LoadOptions& options = {});
// A model from a file on disk.
LoadResult load_gltf_file(const std::string& path, const LoadOptions& options = {});

}  // namespace ge::assets
