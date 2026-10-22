// FALL LINE'S FRAME: the game's pass table over engine/render, and the one object that owns a
// frame's GPU half (docs/rendering.md § The frame). `kPassTable` lists the passes in the order
// they run, with the named targets each reads and writes; `draw` runs exactly that order, and
// `pass_table_faults` holds the order to the engine's check.
//
//   sky.tables      -> sky.transmittance sky.multiscatter sky.view sky.aerial sky.light
//   terrain.cull    -> terrain.tiles terrain.indirect (and terrain.normals, once)
//   terrain.horizon -> terrain.horizon (the bakes due)
//   terrain.depth   -> depth (the prepass)
//   hiz             -> hiz (the depth's pyramid)
//   terrain         -> hdr motion (shaded where the depth is equal)
//   sky             -> hdr motion (behind depth)
//   taa             -> taa.history (at the output's size; with FrameOptions::taa)
//   post            -> final
//
// With TAA the frame renders at FrameOptions::scale of the output's size, jittered, and the
// resolve brings it up to the output; without it, at the output's size, unjittered, and post
// reads the HDR target.
//
// The frame reads the game only through a FrameView (views.hpp until the run exists, M5) and
// the map it was made for. Every pipeline is made in `create_frame` (the manifest), and the
// device's load ends there: a pipeline made after it is an error.
//
// ge:summary Fall Line's frame: its pass table over engine/render and the GPU half it owns
// ge:keywords fall-line frame pass-table game-frame create_frame draw pass_table_faults
// ge:keywords render offscreen surface stats
// ge:skill render
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/taa.hpp"
#include "render/terrain.hpp"

namespace fall_line::app {

struct FrameOptions {
  bool taa = true;
  double scale = 1.0;           // the render size over the output's, with TAA
  bool overdraw_probe = false;  // labs: the terrain's overdraw pipelines
};

struct GameFrame {
  FrameOptions options;
  ge::gpu::PipelineSet pipelines;
  uint32_t output_width = 0, output_height = 0;
  ge::render::FrameTargets targets;  // at the render size
  ge::render::FrameHistory history;
  wgpu::Buffer uniforms;
  ge::render::FrameUniforms staged{};
  wgpu::BindGroupLayout frame_layout;
  wgpu::BindGroup frame_group;
  wgpu::BindGroup post_groups[2];  // over TAA's histories; [0] over the HDR target without TAA
  ge::render::Sky sky;
  ge::render::Terrain terrain;
  ge::render::Hiz hiz;
  ge::render::Taa taa;
  ge::render::Post post;
  ge::render::FrameRecorder recorder;
  std::vector<uint8_t> albedo;
};

// Empty when the pass table's order holds (render/frame.hpp's check).
std::string pass_table_faults();

// The frame for `level`, drawing outputs of width x height in `output` format. Null after an
// error line when a pipeline fails. Ends the device's load.
std::unique_ptr<GameFrame> create_frame(ge::gpu::Device& device,
                                        const ge::mountain::mapgen::Level& level, uint32_t width,
                                        uint32_t height, wgpu::TextureFormat output,
                                        const FrameOptions& options = {});

// New targets for outputs of width x height (a resized window); the history starts over.
void resize(ge::gpu::Device& device, GameFrame& frame, uint32_t width, uint32_t height);

// The camera cut: the next frame keeps nothing of the last (no motion, no history).
void cut(GameFrame& frame);

// Encode and submit one frame of `view` into `output` (a view of a texture of the output format
// at the output's size). `view.frame` indexes the jitter. The stats' GPU times arrive a frame or
// more later.
void draw(ge::gpu::Device& device, GameFrame& frame, const ge::render::FrameView& view,
          const wgpu::TextureView& output);

}  // namespace fall_line::app
