// TAA-U: TEMPORAL ANTI-ALIASING AND UPSCALING (docs/rendering.md § Anti-aliasing and
// upscaling). The scene renders at `scale` of the output's size with the projection shifted by a
// sub-pixel jitter each frame (`taa_jitter`, a Halton (2, 3) cycle); this compute pass resolves
// it into a history at the OUTPUT size. Each output pixel:
//
// - reconstructs this frame's colour from the 3x3 render pixels nearest it, each weighed by its
//   sample's distance (the jitter undone) and by the inverse of its exposed brightness, so a
//   lone bright sample cannot flicker;
// - reprojects the history by the motion target (render/frame.hpp; the motion at the nearest
//   depth of the 3x3, so an edge moves with its foreground) and samples it Catmull-Rom;
// - clips the history into the box of the 3x3's colours (YCoCg, mean +- kTaaClipGamma sigma), so
//   what is no longer there is not ghosted;
// - blends: `alpha` of the new frame, more where the motion target's reactive mask asks for it,
//   all of it where the history is off the screen or after `reset_taa` (a cut, a resize). While
//   the history is young the blend is a plain running mean, so a still converges in one cycle.
//
// The history ping-pongs between two textures; `taa_output` is the one the last resolve wrote,
// which the post pass reads. Nothing here knows a game: the caller picks the scale and says when
// the camera cut.
//
// ge:summary TAA-U: the jitter, the history, the resolve at the output size, the reset
// ge:keywords render taa taa-u temporal-anti-aliasing upscaling jitter halton history reproject
// ge:keywords motion-vectors variance-clip ycocg catmull-rom reactive render-scale
// ge:keywords taa_jitter taa_phases taa_render_size create_taa encode_taa reset_taa taa_output
// ge:skill render
// ge:test tests/render_draw_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr int kTaaMaxPhases = 32;
inline constexpr double kTaaClipGamma = 1.25;

struct TaaSettings {
  double scale = 1.0;  // the render size over the output's, 0.25..1
  double alpha = 0.1;  // the new frame's share of a settled pixel
};

// The jitter cycle's length: 8 at full size, more as the scale drops (each output pixel wants
// about 8 samples), at most kTaaMaxPhases.
int taa_phases(double scale);
// Frame `frame`'s jitter in render pixels (x right, y down), in [-0.5, 0.5): the Halton (2, 3)
// point (frame mod phases) + 1, centred.
void taa_jitter(uint64_t frame, int phases, double out[2]);
// The render size for an output size at `scale` (rounded, at least 1).
uint32_t taa_render_size(uint32_t output, double scale);

struct Taa {
  TaaSettings settings;
  uint32_t width = 0, height = 0;  // the output's size
  wgpu::Texture history[2];
  wgpu::TextureView history_views[2];
  wgpu::Buffer params;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup groups[2];  // groups[k] reads history[1 - k] and writes history[k]
  uint32_t p_resolve = 0;
  int written = 1;  // the history the last resolve wrote
  int age = 0;      // frames resolved since the last reset (0: the next one starts over)
};

// The pipeline (added to the manifest); `frame` is group 0's layout.
void create_taa(gpu::Device& device, gpu::PipelineSet& pipelines, Taa& taa,
                const wgpu::BindGroupLayout& frame, const TaaSettings& settings);
// The histories at the output's size and the groups over `targets` (again after a resize); resets.
void create_taa_targets(gpu::Device& device, Taa& taa, const FrameTargets& targets,
                        uint32_t output_width, uint32_t output_height);
// Start the history over at the next resolve (a cut).
void reset_taa(Taa& taa);
// The resolve ("taa"), after the opaque passes and the sky; uploads its params.
void encode_taa(gpu::Device& device, Taa& taa, const gpu::PipelineSet& pipelines,
                FrameRecorder& rec, wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame);
// The resolved frame (the output's size): what the post pass reads.
const wgpu::TextureView& taa_output(const Taa& taa);

}  // namespace ge::render
