// GPU-DRIVEN INSTANCE FAMILIES (docs/rendering.md § GPU-driven instances): everything that is
// drawn many times (trees, rocks, carriers, figures, poles) is an instance of a mesh in a
// family, culled, LOD-picked and drawn by the GPU. The CPU never walks a family's instances in
// a frame; it writes the instances that moved (`set_instances`) and nothing else.
//
// - A FAMILY holds a table of meshes, each with up to kMaxLods levels of detail (each LOD a
//   packed mesh, render/mesh.hpp, in one shared vertex buffer, its vertices pulled by index), a
//   table of materials (linear albedo; an instance's material plus a vertex's part picks one),
//   and the instances as streams in one storage buffer (structure of arrays): the position and
//   material, the rotation, the scale and mesh, and the last frame's position and rotation (the
//   motion vectors of what moves).
// - A BUCKET is one (mesh, LOD) pair in one phase, one indirect draw. The cull appends each
//   visible instance to its bucket's compacted list and counts it into the draw's arguments.
//   A draw tells its bucket to the vertex stage through its base vertex (bucket << 20), since a
//   portable indirect draw has no first instance: every LOD's vertices are indexed from 0.
// - TWO-PHASE CULLING against the depth pyramid (render/hiz.hpp). The EARLY phase (before the
//   depth prepass) tests each instance's bounding sphere against the frustum and against LAST
//   frame's pyramid, reprojected through the last frame's camera; what that hides it marks for
//   the LATE phase, which runs after this frame's prepass and pyramid and draws the ones this
//   frame shows after all. So a fast turn never leaves a hole for a frame. After a cut (no last
//   frame) the early phase culls by the frustum alone.
// - THE LOD is picked from the sphere's radius on screen in pixels: a LOD is drawn while that is
//   at least its `min_px` (the LODs finest first); under kMinPixels nothing is. Within kLodBand
//   of a threshold both LODs are drawn and each keeps a dithered share of the pixels, in the
//   depth prepass only (its fragment discards; the colour pass tests `equal` and so shades only
//   the pixels the prepass kept), so a LOD change never pops. `pick_lod` is the CPU twin.
//
// - A PALETTE FAMILY (`create_family`'s `palette_joints`): every instance its own palette of
//   joints (render/skinning.hpp's `set_palettes`), and LODs made by `palette_source` posed by it
//   in the vertex stage, this frame's and the last's (a crowd: one mesh, a pose an instance).
//
// - CASTERS: a family that casts into the sun's shadow maps (render/shadows.hpp) has a CASTER
//   SLOT a map it is drawn into (`create_family_casters`): lists and draws of its own, filled by
//   the cull's `shadow` entry (the map's box, no pyramid, the LOD by the radius in the map's
//   texels, no band) and drawn depth only, both faces, by `p_caster`.
//
// The passes, in a frame's order: `encode_instances_cull` early, `encode_instances_depth` early
// (after the ground's prepass), the pyramid, the cull late, the depth late, then the colour pass
// (`encode_instances`, both phases' lists, `equal`). Every family in a span shares each pass.
// Group 1 of the draws is the light (`instance_light_group`: the ground's own shadow from the
// terrain's horizon maps), group 2 the family, group 3 the sun's shadows of what stands
// (`InstancePipelines::shadows`, bound by `bind_instance_shadows`; none until then). Shaders:
// instances.wgsl, instances_cull.wgsl.
//
// ge:summary GPU-driven instance families: meshes and LODs, two-phase Hi-Z culling, indirect
// draws
// ge:keywords render instances instance-family gpu-driven two-phase-culling occlusion-culling
// ge:keywords hi-z lod lod-band dither cross-fade bucket indirect-draw base-vertex compaction
// ge:keywords vertex-pulling soa streams materials pick_lod create_family set_instances
// ge:keywords encode_instances_cull encode_instances_depth encode_instances instance-stats
// ge:keywords casters caster-slot shadow-cull create_family_casters draw_casters
// ge:keywords wire-probe wireframe encode_instances_wire palette-family palettes
// ge:skill render
// ge:test tests/render_instances_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/mesh.hpp"
#include "render/terrain.hpp"

namespace ge::render {

inline constexpr int kMaxLods = 4;           // a mesh's levels of detail
inline constexpr uint32_t kMaxBuckets = 64;  // a family's (mesh, LOD) pairs
inline constexpr float kLodBand = 0.15f;     // the cross-fade's half width, a share of a threshold
inline constexpr float kMinPixels = 0.35f;   // the radius on screen under which nothing is drawn
inline constexpr uint32_t kMaxInstances = 1u << 23;  // a list entry's index bits

// A level of detail: its mesh, and the radius on screen (px) down to which it is drawn. The
// last LOD's is ignored (it is drawn down to kMinPixels). A LOD whose vertices are rewritten on
// the GPU (a skinned mesh, render/skinning.hpp) gives the box its positions are packed into
// (`reach`): the box of every pose, which also bounds the mesh's sphere for the cull; such a LOD
// also keeps its vertices' last frame's positions (`InstanceFamily::lod_last`), so the colour
// pass draws each vertex's own motion.
struct LodReach {
  std::array<float, 3> min;
  std::array<float, 3> size;
};
// A LOD skinned in the vertex stage from each instance's own palette (`skin`, three words a
// vertex: its four joints as bytes, then its four weights as 16-bit unorms; render/skinning.hpp's
// `palette_source`) keeps its bind pose packed into `reach` and no last positions: the draw poses
// every vertex by its instance's palette this frame and the last (`InstanceFamily::palettes`).
struct LodSource {
  MeshData mesh;
  float min_px = 0;
  std::optional<LodReach> reach = std::nullopt;
  std::vector<uint32_t> skin = {};
};
struct MeshSource {
  std::vector<LodSource> lods;  // finest first, at most kMaxLods
};

// An instance as the CPU writes it: a mesh-space point p lands at position + rotate(rotation,
// p * scale). Rotation is a unit quaternion (x, y, z, w), body to world (engine/core/quat.hpp).
struct Instance {
  double position[3] = {0, 0, 0};
  float rotation[4] = {0, 0, 0, 1};
  float scale[3] = {1, 1, 1};
  uint32_t mesh = 0;
  uint32_t material = 0;  // a vertex of part k takes material + k
};

// The LOD for a sphere `radius_px` px on screen across `lods` thresholds (finest first): `lod`,
// and inside a band `other` (the next coarser) with `share` the finer one's part of the pixels.
// `lod` is -1 under kMinPixels.
struct LodPick {
  int lod = -1;
  int other = -1;
  float share = 1;
};
LodPick pick_lod(const float* min_px, int lods, float radius_px);

// What a family's cull did in a frame (instances, each counted once).
struct InstanceStats {
  uint32_t early = 0;     // drawn by the early phase
  uint32_t late = 0;      // hidden by the last frame's pyramid, shown by this one's
  uint32_t frustum = 0;   // outside the frustum
  uint32_t small = 0;     // under kMinPixels
  uint32_t occluded = 0;  // hidden by the last frame's pyramid
  uint32_t hidden = 0;    // and by this frame's: not drawn
};

// The pipelines and layouts every family shares (added to the manifest once).
struct InstancePipelines {
  wgpu::BindGroupLayout cull_layout, draw_layout, light_layout, empty_layout;
  wgpu::BindGroup empty_group;
  wgpu::BindGroup shadows;  // group 3 of the lit draws: render/shadows.hpp's receivers' group
  uint32_t p_early = 0, p_late = 0, p_depth = 0, p_draw = 0;
  uint32_t p_shadow_cull = 0, p_caster = 0;         // a shadow map's cull and depth draw
  uint32_t p_count_equal = 0, p_count_greater = 0;  // the overdraw probe's, when asked for
  bool wire = false;                                // the wire probe's pipeline is made
  uint32_t p_wire = 0;
};
void create_instance_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines,
                               InstancePipelines& ip, const wgpu::BindGroupLayout& frame,
                               bool overdraw_probe = false, bool wire_probe = false);
// Group 1 of the draws: the ground's own shadow, read from the terrain's horizon maps.
wgpu::BindGroup instance_light_group(gpu::Device& device, const InstancePipelines& ip,
                                     const Terrain& terrain);
// The sun's shadows the lit draws read (render/shadows.hpp's `SunShadows::group`).
void bind_instance_shadows(InstancePipelines& ip, const wgpu::BindGroup& shadows);

// A family's draws into one shadow map: its lists, arguments, parameters and groups.
struct CasterSlot {
  wgpu::Buffer params, lists, args;
  wgpu::BindGroup cull_group, draw_group;
  bool culled = false;  // this frame's cull runs (its arguments were reset)
};

struct InstanceFamily {
  std::string label;
  uint32_t capacity = 0, count = 0;
  uint32_t lod_count = 0;
  wgpu::Buffer vertices, indices, meshes, lods, materials, instances, lists, args, state, params;
  // The wire probe's (families made with InstancePipelines::wire): every triangle's three edges
  // as lines, and their draws' arguments, whose instance counts are copied from `args`.
  wgpu::Buffer wire_indices, wire_args;
  wgpu::BindGroup cull_group, draw_group;
  std::vector<uint32_t> args_reset;  // the draws' arguments with no instances, and zero stats
  std::vector<float> streams;        // the CPU's copy of the instance streams
  std::vector<float> lod_px;         // each LOD's min_px
  std::vector<uint32_t> mesh_lod;    // each mesh's first LOD
  std::vector<uint32_t> lod_vertex;  // each LOD's first vertex in `vertices`
  std::vector<uint32_t> lod_last;    // each skinned LOD's last positions' first word, else 0
  std::vector<CasterSlot> casters;   // a shadow map's draws each (create_family_casters)
  bool moved_since_cut = false;      // false: the next write's last frame is the write itself
  // A palette family's (`palette_joints` > 0): every instance's palette, `palette_joints` joints
  // of three vec4 rows each, twice (this frame's half and the last frame's, swapped by each
  // `set_palettes`); `palettes_settle`: the next write is its own last frame too.
  uint32_t palette_joints = 0;
  wgpu::Buffer palettes;
  uint32_t palette_half = 0;
  bool palettes_settle = true;
  // instances_cull.wgsl's `FamilyParams`, filled by prepare.
  struct Params {
    uint32_t info[4];  // count, LODs, capacity, occlusion on
    float cull[4];     // px a unit of tan on screen, kMinPixels, kLodBand, 0
    uint32_t skin[4];  // joints a palette, this frame's first row, the last frame's, 0
  } staged{};
};

// A family of `capacity` instances (≤ kMaxInstances) of `meshes` (≤ kMaxBuckets LODs in all)
// lit by `materials` (linear rgb). Its cull needs the pyramid: `bind_family_hiz` after it is
// made (and again after a resize). `palette_joints` > 0 makes it a palette family: each instance
// has a palette of that many joints (render/skinning.hpp's `set_palettes`), which its LODs made
// by `palette_source` are posed by.
void create_family(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& family,
                   std::string_view label, std::span<const MeshSource> meshes,
                   std::span<const float> materials_rgb, uint32_t capacity,
                   uint32_t palette_joints = 0);
void bind_family_hiz(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& family,
                     const Hiz& hiz);

// Slots for `maps` shadow maps (before `bind_family_hiz`, which makes their cull groups).
void create_family_casters(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& family,
                           int maps);

// Write instances [first, first + n): this frame's place, and the last frame's from the place
// written before (or this one, the first time and after `cut_family`). Grows `count` to cover.
void set_instances(gpu::Device& device, InstanceFamily& family, uint32_t first,
                   std::span<const Instance> instances);
void set_instance_count(InstanceFamily& family, uint32_t count);
// The camera cut: the next write moves nothing since the last frame (nor the next palettes).
void cut_family(InstanceFamily& family);
// A palette family's rows for its parameters' `skin`: joints, this frame's first row, the last's.
void palette_rows(const InstanceFamily& family, uint32_t out[4]);

// The family's parameters for this frame (the screen's scale for the LOD, whether the last
// frame's pyramid may cull) and its draws' arguments reset.
void prepare_family(gpu::Device& device, InstanceFamily& family, const FrameView& view,
                    uint32_t render_height, bool occlusion);

// Caster slot `slot`'s cull this frame, for a map of `texels_per_m` (its arguments reset).
void prepare_family_caster(gpu::Device& device, InstanceFamily& family, int slot,
                           float texels_per_m);

// The cull ("instances.cull", or "instances.cull.late" for the late phase) over every family.
void encode_instances_cull(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                           const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                           wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, bool late);
// The depth prepass of one phase's lists ("instances.depth" / "instances.depth.late"), over the
// frame's depth (loaded).
void encode_instances_depth(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                            const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                            wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                            const wgpu::BindGroup& frame, const wgpu::BindGroup& light, bool late);
// The colour pass ("instances"): both phases' lists into the HDR and motion targets (loaded),
// where the depth is `equal`.
void encode_instances(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                      const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                      wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                      const wgpu::BindGroup& frame, const wgpu::BindGroup& light);
// The overdraw probe (labs; needs create_instance_pipelines' overdraw_probe): every fragment of
// both phases' lists that passes the depth test adds one to `count` (loaded), as
// render/terrain.hpp's probe does.
void encode_instances_overdraw(std::span<InstanceFamily* const> families,
                               const InstancePipelines& ip, const gpu::PipelineSet& pipelines,
                               wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                               const wgpu::BindGroup& frame, const wgpu::BindGroup& light,
                               const wgpu::TextureView& count, bool after_prepass);

// The wire probe (labs; needs create_instance_pipelines' wire_probe): both phases' lists drawn as
// their triangles' edges into `colour` (loaded) over the frame's depth (read only), each line a
// hair nearer than its surface so the hidden ones stay hidden, as render/terrain.hpp's does.
void encode_instances_wire(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                           const gpu::PipelineSet& pipelines, wgpu::CommandEncoder& encoder,
                           const FrameTargets& targets, const wgpu::BindGroup& frame,
                           const wgpu::BindGroup& light, const wgpu::TextureView& colour);

// One shadow map's casters: in a compute pass, `cull_shadow` dispatches slot `slot`'s cull of
// every family (the map's view is group 0, set by the caller); in a depth pass of the map,
// `draw_casters` draws them. Both skip a slot not prepared this frame. Return the dispatches /
// draws.
uint32_t cull_shadow(wgpu::ComputePassEncoder& pass, std::span<InstanceFamily* const> families,
                     int slot);
uint32_t draw_casters(wgpu::RenderPassEncoder& pass, std::span<InstanceFamily* const> families,
                      int slot);

// Labs and tests (blocks): what the family's last cull did.
InstanceStats read_instance_stats(gpu::Device& device, const InstanceFamily& family);
// And what its last cull of caster slot `slot` did (`early`: drawn, `frustum`: outside the map).
InstanceStats read_caster_stats(gpu::Device& device, const InstanceFamily& family, int slot);

}  // namespace ge::render
