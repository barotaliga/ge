// The test scene's pipeline descriptor and its one pass.
//
// ge:summary The test scene's pipeline and pass
// ge:keywords gpu test-scene render-pass draw
// ge:skill gpu
// ge:test tests/gpu_capture_test.cpp
#include "gpu/test_scene.hpp"

#include "gpu/test_scene.wgsl.hpp"

namespace ge::gpu {

uint32_t add_test_scene(Device& device, PipelineSet& pipelines, wgpu::TextureFormat format) {
  const wgpu::ShaderModule module = create_shader(device, wgsl::test_scene, "gpu.test_scene");
  wgpu::ColorTargetState colour{};
  colour.format = format;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "gpu.test_scene";
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.fragment = &fragment;
  return pipelines.add_render(device, desc);
}

void draw_test_scene(wgpu::CommandEncoder& encoder, const wgpu::RenderPipeline& pipeline,
                     const wgpu::TextureView& target, const wgpu::PassTimestampWrites* timestamps) {
  wgpu::RenderPassColorAttachment attachment{};
  attachment.view = target;
  attachment.loadOp = wgpu::LoadOp::Clear;
  attachment.storeOp = wgpu::StoreOp::Store;
  attachment.clearValue = {0, 0, 0, 1};
  wgpu::RenderPassDescriptor pass_desc{};
  pass_desc.label = "gpu.test_scene";
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &attachment;
  pass_desc.timestampWrites = timestamps;
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_desc);
  pass.SetPipeline(pipeline);
  pass.Draw(6);
  pass.End();
}

}  // namespace ge::gpu
