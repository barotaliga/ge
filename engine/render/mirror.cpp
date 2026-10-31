// The kept scene's texture, pipeline and pass.
//
// ge:summary The mirror's kept scene: its texture, pipeline, group and pass
// ge:keywords render mirror reflection mirror-keep pipeline pass
// ge:skill render
// ge:test tests/render_mirror_test.cpp
#include "render/mirror.hpp"

#include "render/mirror_keep.wgsl.hpp"

namespace ge::render {

void create_mirror(gpu::Device& device, gpu::PipelineSet& pipelines, Mirror& mirror) {
  mirror.layout = make_layout(device, "mirror.keep", {Slot::texture, Slot::depth, Slot::uniform},
                              wgpu::ShaderStage::Fragment);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::mirror_keep, "mirror.keep");
  wgpu::ColorTargetState colour{};
  colour.format = kHdrFormat;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "mirror.keep";
  desc.layout = make_pipeline_layout(device, "mirror.keep", {mirror.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.fragment = &fragment;
  mirror.p_keep = pipelines.add_render(device, desc);
}

void create_mirror_targets(gpu::Device& device, Mirror& mirror, const FrameTargets& targets,
                           const wgpu::Buffer& uniforms) {
  mirror.last = make_texture(device, "mirror.last", targets.width, targets.height, 1, kHdrFormat,
                             wgpu::TextureUsage::RenderAttachment |
                                 wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc);
  mirror.last_view = mirror.last.CreateView();
  mirror.group = make_group(device, "mirror.keep", mirror.layout,
                            {targets.hdr_view, targets.depth_view, uniforms});
}

void encode_mirror(const Mirror& mirror, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = mirror.last_view;
  colour.loadOp = wgpu::LoadOp::Clear;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "mirror.keep");
  pass.SetPipeline(pipelines.render(mirror.p_keep));
  pass.SetBindGroup(0, mirror.group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
