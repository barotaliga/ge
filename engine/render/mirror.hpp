// THE SCENE IN A MIRROR (docs/rendering.md § Reflections, M38c): a screen-space reflection any
// surface asks for through one group-0 helper (render/wgsl/mirror.wgsl's `scene_reflection`).
//
// - THE PASS: "mirror.keep" keeps the frame's colour before TAA-U and, in alpha, each pixel's
//   distance from the eye (render/mirror_keep.wgsl), in one RGBA16F texture the size of the
//   render; it runs after the last pass that draws the world (the bolts) and before TAA-U.
// - THE READ: the NEXT frame binds that texture at group 0's binding 15 (`frame_group`'s
//   `mirror`). A surface's reflected ray is marched in the last frame's view (the eye's move and
//   `prev_view_proj` put each step on its pixel), halved down onto where it meets what the last
//   frame drew, and the colour there read, blurred by the surface's roughness; the share it
//   counts for fades toward the screen's edges and the march's end, and where the ray leaves the
//   picture the caller's own sky stands. A frame lag no one sees at 60 Hz buys a reflection of
//   EVERYTHING the frame draws (the water, the particles, the rain), and no extra binding for
//   the depth.
// - OFF: a group bound with the sky's 1 x 1 blank (every lab and test that never keeps the
//   scene) reflects nothing; the first frame after the targets are made reads zeros (nothing
//   drawn) and does the same.
//
// The wet ground's `wet_sheen` (render/wgsl/frame.wgsl) is its first reader: a puddle mirrors the
// scene and the rain's film a blurred one.
//
// ge:summary Screen-space reflections: the scene kept a frame with its distances, marched by a
// helper ge:keywords render mirror reflection ssr screen-space-reflection puddle wet-ground
// mirror-keep ge:keywords create_mirror create_mirror_targets encode_mirror scene_reflection
// last-frame ge:skill render ge:test tests/render_mirror_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

struct Mirror {
  wgpu::BindGroupLayout layout;
  uint32_t p_keep = 0;
  wgpu::Texture last;  // RGBA16F: rgb the colour, a the distance (m; 0: nothing drawn)
  wgpu::TextureView last_view;
  wgpu::BindGroup group;
};

// The pipeline into the manifest.
void create_mirror(gpu::Device& device, gpu::PipelineSet& pipelines, Mirror& mirror);
// The kept scene at the targets' size, and the pass's group (again when the targets change).
// `uniforms` is the frame block's buffer.
void create_mirror_targets(gpu::Device& device, Mirror& mirror, const FrameTargets& targets,
                           const wgpu::Buffer& uniforms);
// "mirror.keep": the HDR target and the depth into `Mirror::last`.
void encode_mirror(const Mirror& mirror, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder);

}  // namespace ge::render
