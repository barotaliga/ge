// THE WORLD'S CULL (r_cull): which of a map's faces a frame draws. Each face is a CHUNK (its run of
// triangles in the renderer's kind-sorted indices, its box, its kind). On the CPU, the eye's leaf
// picks the POTENTIALLY VISIBLE SET (brush/map.hpp's `potentially_visible`; a brush model's faces,
// a door's or a lift's, are always in it). On the GPU (bsp_cull.wgsl), in TWO PHASES round the
// depth pyramid (render/hiz.hpp): the EARLY phase draws the set's faces in the frustum that LAST
// frame's pyramid does not hide; then the pyramid is built from that depth, and the LATE phase
// draws the faces the early one held back that this frame's pyramid shows after all. So a fast
// turn never leaves a hole. Each phase compacts its faces' triangles by a scan in the faces' own
// order, so the frame's pixels never hang on the order the GPU's threads ran in, and draws a kind
// with one indirect draw.
//
// The passes: "bsp.cull" (the early phase's three dispatches), the renderer's "bsp", "hiz",
// "bsp.cull.late" and "bsp.late" (bsp_draw.hpp's `encode_bsp`).
//
// ge:summary The world's cull: the eye's PVS on the CPU, the frustum and Hi-Z in two GPU phases
// ge:keywords bsp cull r_cull pvs potentially-visible hi-z occlusion two-phase indirect chunk scan
// ge:skill brush render
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"

namespace ge::brush::app {

struct GpuChunk {
  float lo[3];
  uint32_t first;
  float hi[3];
  uint32_t count;
  uint32_t info[4];  // the kind
};

struct BspCull {
  uint32_t chunks = 0, total = 0;  // faces drawn, indices in the source
  std::vector<uint32_t> surface;   // each chunk's map surface
  std::vector<uint8_t> world;      // each chunk: 1 the world's (the PVS decides), 0 a model's
  wgpu::Buffer src, chunk_buf, pvs, dst, sizes, state, args, params;
  std::vector<uint32_t> args_reset;
  render::Hiz hiz;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;  // with the pyramid (create_bsp_cull_targets)
  uint32_t p_test_early = 0, p_scan_early = 0, p_copy_early = 0;
  uint32_t p_test_late = 0, p_scan_late = 0, p_copy_late = 0;
  // This frame's (prepare_bsp_cull).
  uint32_t eye_leaf = 0;
  uint32_t in_set = 0;  // chunks in the potentially visible set
  std::vector<uint8_t> seen;
  std::vector<uint32_t> bits;
};

// What the last frame's cull did, in chunks (faces).
struct CullStats {
  uint32_t chunks = 0;
  uint32_t pvs = 0;              // left out by the potentially visible set
  uint32_t frustum = 0;          // outside the frustum
  uint32_t occluded = 0;         // hidden by the last frame's pyramid (tested again late)
  uint32_t hidden = 0;           // hidden by this frame's pyramid too
  uint32_t early = 0, late = 0;  // drawn by each phase
};

// The chunks for `scene` over the renderer's kind-sorted `src` (made with Storage use): `chunk_of`
// (first, count, kind, surface) in the source's order. The pipelines go to the manifest.
void create_bsp_cull(gpu::Device& device, gpu::PipelineSet& pipelines, BspCull& cull,
                     const Scene& scene, const std::vector<std::array<uint32_t, 4>>& chunk_of,
                     const wgpu::Buffer& src, const wgpu::BindGroupLayout& frame);
// The pyramid at the targets' size, and the group that reads it (again when they change).
void create_bsp_cull_targets(gpu::Device& device, BspCull& cull,
                             const render::FrameTargets& targets);
// This frame's set from the eye (engine metres; `pvs` false: every face), the arguments reset.
void prepare_bsp_cull(gpu::Device& device, BspCull& cull, const Scene& scene, const double eye[3],
                      bool pyramid, bool pvs = true);
// A phase's three dispatches (0 early, 1 late).
void encode_bsp_cull(const BspCull& cull, const gpu::PipelineSet& pipelines,
                     render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                     const wgpu::BindGroup& frame, int phase);
// A phase's draw for kind `k`: the offset of its indirect arguments in `args`.
inline uint64_t cull_args_at(int phase, int kind) { return uint64_t(phase * kKinds + kind) * 20; }

// Labs and tests (block): the last frame's counts.
CullStats read_bsp_cull(gpu::Device& device, const BspCull& cull);

}  // namespace ge::brush::app
