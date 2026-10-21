// The post pass's pipeline, group and draw.
//
// ge:summary The post pass's pipeline and draw
// ge:keywords render post pipeline srgb override
// ge:skill render
// ge:test tests/render_frame_test.cpp
#include "render/post.hpp"

#include "render/post.wgsl.hpp"

namespace ge::render {

void create_post(gpu::Device& device, gpu::PipelineSet& pipelines, Post& post,
                 const wgpu::BindGroupLayout& frame, wgpu::TextureFormat output) {
  post.output = output;
  post.layout = make_layout(device, "post", {Slot::texture}, wgpu::ShaderStage::Fragment);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::post, "post");
  const bool srgb_target = output == wgpu::TextureFormat::RGBA8UnormSrgb ||
                           output == wgpu::TextureFormat::BGRA8UnormSrgb;
  wgpu::ConstantEntry encode{};
  encode.key = "kEncodeSrgb";
  encode.value = srgb_target ? 0.0 : 1.0;
  wgpu::ColorTargetState colour{};
  colour.format = output;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.constantCount = 1;
  fragment.constants = &encode;
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "post.draw";
  desc.layout = make_pipeline_layout(device, "post.draw", {frame, post.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.fragment = &fragment;
  post.p_draw = pipelines.add_render(device, desc);
}

wgpu::BindGroup post_group(gpu::Device& device, const Post& post, const wgpu::TextureView& hdr) {
  return make_group(device, "post", post.layout, {hdr});
}

void encode_post(const Post& post, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame,
                 const wgpu::BindGroup& group, const wgpu::TextureView& output) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = output;
  colour.loadOp = wgpu::LoadOp::Clear;
  colour.storeOp = wgpu::StoreOp::Store;
  colour.clearValue = {0, 0, 0, 1};
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "post");
  pass.SetPipeline(pipelines.render(post.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
