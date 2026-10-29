// Motion blur's tile pipelines, targets and dispatches.
//
// ge:summary Motion blur's tile pipelines, targets and the two dispatches
// ge:keywords render motion-blur tiles neighbour-max pipeline dispatch
// ge:skill render
// ge:test tests/render_motion_blur_test.cpp
#include "render/motion_blur.hpp"

#include <algorithm>

#include "render/motion_blur.wgsl.hpp"

namespace ge::render {

void motion_blur_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h) {
  *w = std::max((width + kTile - 1) / kTile, 1u);
  *h = std::max((height + kTile - 1) / kTile, 1u);
}

void create_motion_blur_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines,
                                  MotionBlur& blur) {
  blur.layout = make_layout(device, "motion_blur", {Slot::texture, Slot::store_2d},
                            wgpu::ShaderStage::Compute);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::motion_blur, "motion_blur");
  const wgpu::PipelineLayout layout = make_pipeline_layout(device, "motion_blur", {blur.layout});
  wgpu::ComputePipelineDescriptor desc{};
  desc.layout = layout;
  desc.compute.module = module;
  desc.label = "motion_blur.tiles";
  desc.compute.entryPoint = "tiles";
  blur.p_tiles = pipelines.add_compute(device, desc);
  desc.label = "motion_blur.near";
  desc.compute.entryPoint = "near";
  blur.p_near = pipelines.add_compute(device, desc);
}

void create_motion_blur_targets(gpu::Device& device, MotionBlur& blur, uint32_t width,
                                uint32_t height, const wgpu::TextureView& motion) {
  motion_blur_size(width, height, &blur.width, &blur.height);
  const auto usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::CopySrc;
  blur.tiles =
      make_texture(device, "motion_blur.tiles", blur.width, blur.height, 1, kHdrFormat, usage);
  blur.near =
      make_texture(device, "motion_blur.near", blur.width, blur.height, 1, kHdrFormat, usage);
  blur.tiles_view = blur.tiles.CreateView();
  blur.near_view = blur.near.CreateView();
  blur.tile_group = make_group(device, "motion_blur.tiles", blur.layout, {motion, blur.tiles_view});
  blur.near_group =
      make_group(device, "motion_blur.near", blur.layout, {blur.tiles_view, blur.near_view});
}

void encode_motion_blur(const MotionBlur& blur, const gpu::PipelineSet& pipelines,
                        FrameRecorder& rec, wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "motion_blur.tiles");
  pass.SetPipeline(pipelines.compute(blur.p_tiles));
  pass.SetBindGroup(0, blur.tile_group);
  pass.DispatchWorkgroups((blur.width + 7) / 8, (blur.height + 7) / 8);
  pass.SetPipeline(pipelines.compute(blur.p_near));
  pass.SetBindGroup(0, blur.near_group);
  pass.DispatchWorkgroups((blur.width + 7) / 8, (blur.height + 7) / 8);
  rec.current().dispatches += 2;
  pass.End();
}

}  // namespace ge::render
