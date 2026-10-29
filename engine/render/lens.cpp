// The lens pass's size, pipeline, target and dispatch.
//
// ge:summary The lens pass's pipeline, quarter-size target, strengths and dispatch
// ge:keywords render lens sun-rays flare pipeline target dispatch
// ge:skill render
// ge:test tests/render_lens_test.cpp
#include "render/lens.hpp"

#include <algorithm>

#include "render/lens.wgsl.hpp"

namespace ge::render {

void lens_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h) {
  *w = std::max((width + 3) / 4, 1u);
  *h = std::max((height + 3) / 4, 1u);
}

void create_lens_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Lens& lens,
                           const wgpu::BindGroupLayout& frame, const LensSettings& settings) {
  lens.settings = settings;
  lens.params = make_buffer(device, "lens.params", 16, wgpu::BufferUsage::Uniform);
  set_lens(device, lens, settings);
  lens.layout = make_layout(
      device, "lens", {Slot::texture, Slot::sampler, Slot::depth, Slot::store_2d, Slot::uniform},
      wgpu::ShaderStage::Compute);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::lens, "lens");
  wgpu::ConstantEntry constants[2] = {};
  constants[0].key = "kRays";
  constants[0].value = settings.rays > 0 ? 1.0 : 0.0;
  constants[1].key = "kFlare";
  constants[1].value = settings.flare > 0 ? 1.0 : 0.0;
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "lens";
  desc.layout = make_pipeline_layout(device, "lens", {frame, lens.layout});
  desc.compute.module = module;
  desc.compute.entryPoint = "main";
  desc.compute.constantCount = 2;
  desc.compute.constants = constants;
  lens.p_lens = pipelines.add_compute(device, desc);
}

void create_lens_targets(gpu::Device& device, Lens& lens, uint32_t width, uint32_t height,
                         const wgpu::TextureView inputs[2], const wgpu::TextureView& depth) {
  lens_size(width, height, &lens.width, &lens.height);
  lens.target = make_texture(device, "lens", lens.width, lens.height, 1, kHdrFormat,
                             wgpu::TextureUsage::StorageBinding |
                                 wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc);
  lens.view = lens.target.CreateView();
  wgpu::SamplerDescriptor sd{};
  sd.label = "lens.linear";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  const wgpu::Sampler sampler = device.device.CreateSampler(&sd);
  for (int k = 0; k < 2; ++k) {
    lens.groups[k] = make_group(device, "lens", lens.layout,
                                {inputs[k], sampler, depth, lens.view, lens.params});
  }
}

void set_lens(gpu::Device& device, const Lens& lens, const LensSettings& settings) {
  const float block[4] = {static_cast<float>(std::clamp(settings.rays, 0.0, 1.0)),
                          static_cast<float>(std::clamp(settings.flare, 0.0, 1.0)), 0, 0};
  device.queue.WriteBuffer(lens.params, 0, block, sizeof block);
}

void encode_lens(const Lens& lens, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, int which) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "lens");
  pass.SetPipeline(pipelines.compute(lens.p_lens));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, lens.groups[which]);
  pass.DispatchWorkgroups((lens.width + 7) / 8, (lens.height + 7) / 8);
  ++rec.current().dispatches;
  pass.End();
}

}  // namespace ge::render
