// Ambient occlusion's pipeline, target, block and dispatch.
//
// ge:summary Ambient occlusion's pipeline, half-size target, block and dispatch
// ge:keywords render ambient-occlusion ao gtao pipeline dispatch half-size
// ge:skill render
// ge:test tests/render_ao_test.cpp
#include "render/ao.hpp"

#include <algorithm>

#include "render/ao.wgsl.hpp"
#include "render/ao_blur.wgsl.hpp"

namespace ge::render {

void ao_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h) {
  *w = std::max((width + 1) / 2, 1u);
  *h = std::max((height + 1) / 2, 1u);
}

void create_ao_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Ao& ao) {
  ao.params = make_buffer(device, "ao.params", 16, wgpu::BufferUsage::Uniform);
  ao.layout = make_layout(device, "ao", {Slot::uniform, Slot::uniform, Slot::depth, Slot::store_2d},
                          wgpu::ShaderStage::Compute);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::ao, "ao");
  wgpu::ConstantEntry constants[2] = {};
  constants[0].key = "kSlices";
  constants[0].value = kAoSlices;
  constants[1].key = "kSteps";
  constants[1].value = kAoSteps;
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "ao";
  desc.layout = make_pipeline_layout(device, "ao", {ao.layout});
  desc.compute.module = module;
  desc.compute.entryPoint = "main";
  desc.compute.constantCount = 2;
  desc.compute.constants = constants;
  ao.pipeline = pipelines.add_compute(device, desc);
  ao.blur_layout =
      make_layout(device, "ao.blur", {Slot::texture, Slot::store_2d}, wgpu::ShaderStage::Compute);
  wgpu::ComputePipelineDescriptor blur{};
  blur.label = "ao.blur";
  blur.layout = make_pipeline_layout(device, "ao.blur", {ao.blur_layout});
  blur.compute.module = gpu::create_shader(device, wgsl::ao_blur, "ao_blur");
  blur.compute.entryPoint = "blur";
  ao.p_blur = pipelines.add_compute(device, blur);
}

void create_ao_targets(gpu::Device& device, Ao& ao, uint32_t width, uint32_t height,
                       const wgpu::Buffer& frame_uniforms, const wgpu::TextureView& depth) {
  ao_size(width, height, &ao.width, &ao.height);
  const auto usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::CopySrc;
  ao.raw = make_texture(device, "ao.raw", ao.width, ao.height, 1, kHdrFormat, usage);
  ao.target = make_texture(device, "ao", ao.width, ao.height, 1, kHdrFormat, usage);
  ao.raw_view = ao.raw.CreateView();
  ao.view = ao.target.CreateView();
  ao.group = make_group(device, "ao", ao.layout, {frame_uniforms, ao.params, depth, ao.raw_view});
  ao.blur_group = make_group(device, "ao.blur", ao.blur_layout, {ao.raw_view, ao.view});
}

void set_ao(gpu::Device& device, const Ao& ao, const AoSettings& settings) {
  const float block[4] = {static_cast<float>(std::max(settings.radius_m, 0.05)),
                          static_cast<float>(std::clamp(settings.strength, 0.0, 1.0)), 0, 0};
  device.queue.WriteBuffer(ao.params, 0, block, sizeof block);
}

void encode_ao(const Ao& ao, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
               wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "ao");
  pass.SetPipeline(pipelines.compute(ao.pipeline));
  pass.SetBindGroup(0, ao.group);
  pass.DispatchWorkgroups((ao.width + 7) / 8, (ao.height + 7) / 8);
  pass.SetPipeline(pipelines.compute(ao.p_blur));
  pass.SetBindGroup(0, ao.blur_group);
  pass.DispatchWorkgroups((ao.width + 7) / 8, (ao.height + 7) / 8);
  rec.current().dispatches += 2;
  pass.End();
}

}  // namespace ge::render
