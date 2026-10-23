// SKINNING ON THE GPU (docs/rendering.md § Particles and figures): a skinned mesh
// (assets/model.hpp) is posed once a frame by a compute pass that writes its vertices, packed,
// into an instance family's vertex buffer (render/instances.hpp), so every view that draws the
// family (the main view's prepass and colour pass, the sun's cascades, the shadow atlas) reads
// the same posed mesh and the family's cull, LODs and casters work on it unchanged.
//
// - THE MESH'S SLOT: `skinned_source` makes the family's LOD for the mesh (its bind pose,
//   packed into its REACH box, assets/pose.hpp's `reach_box`); `create_skin` then ties a `Skin`
//   to that LOD's vertices in the family. A family's mesh is one pose: every instance of it is
//   drawn in it, so each figure that moves on its own is a mesh of its own (the player and the
//   rivals: a handful). A crowd skinned in the vertex stage from per-instance palettes is a
//   later row (docs/goals.md M8e2).
// - THE PALETTE (assets/pose.hpp's `skin_palette`, three vec4 rows a joint) is written by
//   `set_palette` when the pose changes; `encode_skinning` runs the pass ("skinning", one
//   dispatch a skin) before the frame's culls.
// - THE PACK is render/mesh.hpp's to the bit but for rounding a tie (the shader rounds half to
//   even, the CPU half away from zero): `skin_packed` is the CPU twin the test holds the pass to,
//   within one 16-bit step.
// - THE MOTION: before the pass writes a vertex it keeps the position it replaces as the
//   vertex's last frame's (two words a vertex after the family's vertices, `lod_last` in
//   render/instances.hpp), and the colour pass draws each vertex's motion from there, so a limb
//   that swings between frames carries its own motion into TAA. `settle_skin` (a cut, a still,
//   a new skier in the slot) makes the next pass's last frame its own pose: no motion.
//
// ge:summary GPU skinning: a compute pass poses a skinned mesh into an instance family's vertices
// ge:keywords render skinning skinned-mesh compute bone-palette set_palette encode_skinning
// ge:keywords create_skin skinned_source skin_packed reach-box linear-blend-skinning figure
// ge:skill render assets
// ge:test tests/render_skinning_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "assets/model.hpp"
#include "assets/pose.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/instances.hpp"

namespace ge::render {

struct SkinPipelines {
  wgpu::BindGroupLayout layout;
  uint32_t p_skin = 0;
};
void create_skin_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, SkinPipelines& sp);

// The family LOD for `mesh`: its bind pose packed into `reach`.
LodSource skinned_source(const assets::Mesh& mesh, const assets::Box& reach, float min_px = 0);

struct Skin {
  uint32_t vertices = 0, joints = 0;
  uint32_t last = 0;   // the first word of its vertices' last positions in the family's buffer
  bool settle = true;  // the next pass's last frame is its own pose (written by `set_palette`)
  wgpu::Buffer bind, palette, params;
  wgpu::BindGroup group;
  std::vector<float> rows;  // the palette as written: 12 floats a joint
};

// The skin of LOD `lod` of mesh `mesh_index` in `family` (made from `skinned_source(mesh, reach)`),
// with a palette of `joints` joints, at rest (the identity) until the first `set_palette`.
void create_skin(gpu::Device& device, const SkinPipelines& sp, Skin& skin,
                 const InstanceFamily& family, uint32_t mesh_index, uint32_t lod,
                 const assets::Mesh& mesh, const assets::Box& reach, uint32_t joints);

// This frame's palette (assets/pose.hpp's `skin_palette`), and whether the pass after it settles.
// Call it every frame the skin is posed: a settle written stays until the next call.
void set_palette(gpu::Device& device, Skin& skin, std::span<const assets::Affine> palette);
// The next pose carries no motion from the last (a camera cut, a still, a slot given to another).
void settle_skin(Skin& skin);

// The pass ("skinning"): every skin's vertices posed into its family. Before the culls.
void encode_skinning(std::span<Skin* const> skins, const SkinPipelines& sp,
                     const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder);

// Labs and tests (blocks): `count` packed vertices of `family` from vertex `first`.
std::vector<uint32_t> read_family_vertices(gpu::Device& device, const InstanceFamily& family,
                                           uint32_t first, uint32_t count);

// Labs and tests (blocks): the last frame's positions of `skin`'s vertices, two words each.
std::vector<uint32_t> read_skin_last(gpu::Device& device, const InstanceFamily& family,
                                     const Skin& skin);

// The CPU twin: `mesh` skinned by `palette` and packed into `reach`, three words a vertex.
std::vector<uint32_t> skin_packed(const assets::Mesh& mesh, std::span<const assets::Affine> palette,
                                  const assets::Box& reach);

}  // namespace ge::render
