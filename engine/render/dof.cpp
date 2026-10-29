// Depth of field's pipeline, target, block and dispatch.
//
// ge:summary Depth of field's pipeline, half-size target, block and dispatch
// ge:keywords render depth-of-field dof pipeline dispatch half-size
// ge:skill render
// ge:test tests/render_dof_test.cpp
#include "render/dof.hpp"

#include <algorithm>

#include "render/dof.wgsl.hpp"

namespace ge::render {

void dof_size(uint32_t width, uint32_t height, uint32_t* w, uint32_t* h) {
  *w = std::max((width + 1) / 2, 1u);
  *h = std::max((height + 1) / 2, 1u);
}

void create_dof_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Dof& dof,
                          const wgpu::BindGroupLayout& frame) {
  dof.params = make_buffer(device, "dof.params", 16, wgpu::BufferUsage::Uniform);
  dof.layout = make_layout(
      device, "dof", {Slot::texture, Slot::sampler, Slot::depth, Slot::store_2d, Slot::uniform},
      wgpu::ShaderStage::Compute);
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "dof";
  desc.layout = make_pipeline_layout(device, "dof", {frame, dof.layout});
  desc.compute.module = gpu::create_shader(device, wgsl::dof, "dof");
  desc.compute.entryPoint = "main";
  dof.pipeline = pipelines.add_compute(device, desc);
}

void create_dof_targets(gpu::Device& device, Dof& dof, uint32_t width, uint32_t height,
                        const wgpu::TextureView inputs[2], const wgpu::TextureView& depth) {
  dof_size(width, height, &dof.width, &dof.height);
  dof.target = make_texture(device, "dof", dof.width, dof.height, 1, kHdrFormat,
                            wgpu::TextureUsage::StorageBinding |
                                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc);
  dof.view = dof.target.CreateView();
  wgpu::SamplerDescriptor sd{};
  sd.label = "dof.linear";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  const wgpu::Sampler sampler = device.device.CreateSampler(&sd);
  for (int k = 0; k < 2; ++k) {
    dof.groups[k] =
        make_group(device, "dof", dof.layout, {inputs[k], sampler, depth, dof.view, dof.params});
  }
}

void set_dof(gpu::Device& device, const Dof& dof, const DofSettings& settings) {
  const float block[4] = {static_cast<float>(std::max(settings.focus_m, 0.1)),
                          static_cast<float>(std::clamp(settings.sharp, 0.0, 0.95)),
                          static_cast<float>(std::clamp(settings.strength, 0.0, 1.0) * kDofWidest),
                          0};
  device.queue.WriteBuffer(dof.params, 0, block, sizeof block);
}

void encode_dof(const Dof& dof, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, int which) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "dof");
  pass.SetPipeline(pipelines.compute(dof.pipeline));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, dof.groups[which]);
  pass.DispatchWorkgroups((dof.width + 7) / 8, (dof.height + 7) / 8);
  ++rec.current().dispatches;
  pass.End();
}

}  // namespace ge::render
