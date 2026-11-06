// THE SKELETAL MODEL FILE (`.iqm`, version 2): a mod's skinned model, with its joints, its
// meshes and its animations, read into a `Model` (assets/model.hpp) in the engine's frame at load
// time, so it poses and skins as an authored model does (assets/pose.hpp, render/skinning.hpp).
// And written back from one, so the tests and the labs make files in code, and a model the engine
// holds can be handed to a tool that reads the format.
//
// THE FILE: a 124-byte header (the magic `INTERQUAKEMODEL` and a NUL, the version, the size,
// then a count and an offset for each lump), all little endian:
// - the TEXT lump, the names (joints, meshes, materials, animations) as NUL-ended strings, each
//   named by its offset into the lump;
// - MESHES (24 bytes each: name, material, first vertex and count, first triangle and count),
//   each a run of the shared vertices and of the triangles;
// - VERTEX ARRAYS (20 bytes each: type, flags, format, components, offset): the position (float
//   3), the texture coordinates (float 2), the normal (float 3), the blend indices (ubyte 4) and
//   the blend weights (ubyte 4 read as /255, or float 4); a tangent or a colour is passed over;
// - TRIANGLES, three vertex indices each (absolute, not the mesh's own);
// - JOINTS (48 bytes each: name, parent or -1, translation, rotation x y z w, scale) at rest,
//   each local to its parent, parents before children;
// - POSES (88 bytes each, one a joint: the parent, a mask of the ten channels a frame stores, an
//   offset and a scale a channel): a frame's channel is offset + scale * its stored ushort when
//   its mask bit is set, the offset alone when not;
// - ANIMATIONS (20 bytes each: name, first frame and count, frames a second, loop flag) over
//   the FRAMES lump, `num_framechannels` ushorts a frame.
//
// THE FRAME. The file is right-handed with +z up, its front toward +x and +y its left; the
// engine's +x is the right, +y up and +z the front (engine/core/math.hpp). A point (x, y, z) comes
// in as (-y, z, x) times `metres`: a signed swap of the axes with a mirror in it, so a rotation
// (x, y, z, w) comes in as (y, -z, -x, w) and a scale (x, y, z) as (y, z, x). Which way round a
// triangle faces is read off the file's normals (most triangles' faces agree with their
// vertices' normals, else every triangle's last two indices swap); a file without normals is
// taken to face as the mirror leaves it and gets smooth normals made from its faces.
//
// THE MODEL. One mesh a file's mesh, a run of meshes of one name joined into one (so a mesh of
// several materials, written as a mesh a material, reads back whole), its vertices renumbered from
// 0, each run's material its triangles' part (materials shared by name, `Material::name` the
// file's material string, which a game reads as a texture's name); every mesh's texture
// coordinates kept (`Mesh::uv`); a file with no joints gets one root joint and every vertex bound
// to it. Each animation is baked at kClipRateHz from its frames (translations and scales on the
// line, rotations on the arc); a looping one gets a last frame that is its first again, so a clip
// played round meets itself.
//
// A FAULT names the file and what is wrong (a lump past the end, an index past its array, a
// joint before its parent, more than kMaxJoints) and the model is not read.
//
// ge:summary The skeletal model file (.iqm v2): read into a Model in the engine's frame, written
// ge:keywords assets iqm skeletal-model skeletal skinned-model mod-model load_iqm write_iqm
// ge:keywords joints poses frames framechannels animation bake vertex-arrays blend-indices
// ge:keywords blend-weights handedness mirror winding classic-frame z-up
// ge:skill assets
// ge:test tests/assets_iqm_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/model.hpp"

namespace ge::assets {

struct IqmOptions {
  double metres = 1;  // one of the file's units, m (a map unit's model: 0.025)
};

// A model from a file's bytes; `where` names it in the faults.
LoadResult load_iqm(std::span<const uint8_t> bytes, std::string_view where,
                    const IqmOptions& options = {});
// A model from a file on disk.
LoadResult load_iqm_file(const std::string& path, const IqmOptions& options = {});

// The model as a file: every mesh, its joints at rest, each clip as an animation at kClipRateHz
// (its frames' channels stored at 16 bits over each channel's range, so a frame read back is
// within a 65535th of that range). A model's morph targets are not written.
std::vector<uint8_t> write_iqm(const Model& model, const IqmOptions& options = {});

}  // namespace ge::assets
