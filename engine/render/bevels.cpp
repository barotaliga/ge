// Bevelled edges in the picture (bevels.hpp).
//
// ge:summary The bevels' pipeline, settings, group and pass
// ge:keywords render bevels pipeline pass multiply-blend
// ge:skill render
// ge:test tests/render_bevels_test.cpp
#include "render/bevels.hpp"

#include <cstdint>

#include "core/libm/libm.hpp"
#include "render/bevels.wgsl.hpp"

namespace ge::render {

namespace {

struct BevelBlock {
  float radius_m, strength, min_cos, pad;
};

}  // namespace

void create_bevels(gpu::Device& device, gpu::PipelineSet& pipelines, Bevels& bevels) {
  bevels.layout = make_layout(device, "bevels", {Slot::depth, Slot::uniform, Slot::uniform},
                              wgpu::ShaderStage::Fragment);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bevels, "bevels");
  // The colour times the pixel's change of light: src * dst.
  wgpu::BlendState blend{};
  blend.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Dst, wgpu::BlendFactor::Zero};
  blend.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
  wgpu::ColorTargetState colour{};
  colour.format = kHdrFormat;
  colour.blend = &blend;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "bevels";
  desc.layout = make_pipeline_layout(device, "bevels", {bevels.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.fragment = &fragment;
  bevels.p_draw = pipelines.add_render(device, desc);
  bevels.settings = make_buffer(device, "bevels.settings", sizeof(BevelBlock),
                                wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
  set_bevels(device, bevels, {});
}

void create_bevel_targets(gpu::Device& device, Bevels& bevels, const FrameTargets& targets,
                          const wgpu::Buffer& uniforms) {
  bevels.group =
      make_group(device, "bevels", bevels.layout, {targets.depth_view, uniforms, bevels.settings});
}

void set_bevels(gpu::Device& device, Bevels& bevels, const BevelSettings& s) {
  const BevelBlock b{static_cast<float>(s.radius_m), static_cast<float>(s.strength),
                     static_cast<float>(core::libm::cos(kBevelMinDeg * 3.14159265358979 / 180)), 0};
  device.queue.WriteBuffer(bevels.settings, 0, &b, sizeof b);
}

void encode_bevels(const Bevels& bevels, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = targets.hdr_view;
  colour.loadOp = wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "bevels");
  pass.SetPipeline(pipelines.render(bevels.p_draw));
  pass.SetBindGroup(0, bevels.group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
