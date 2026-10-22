// The post pass's pipeline, group and draw, and the grade's table.
//
// ge:summary The post pass's pipeline, draw and the grade's lookup table upload
// ge:keywords render post pipeline srgb override grade lut upload half-float
// ge:skill render
// ge:test tests/render_post_test.cpp
#include "render/post.hpp"

#include <string>

#include "core/output.hpp"

#include "render/post.wgsl.hpp"

namespace ge::render {

void create_post(gpu::Device& device, gpu::PipelineSet& pipelines, Post& post,
                 const wgpu::BindGroupLayout& frame, wgpu::TextureFormat output,
                 const PostSettings& settings) {
  post.settings = settings;
  post.output = output;
  post.layout =
      make_layout(device, "post", {Slot::texture, Slot::texture, Slot::sampler, Slot::texture_3d},
                  wgpu::ShaderStage::Fragment);
  wgpu::SamplerDescriptor sd{};
  sd.label = "post.linear";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  post.sampler = device.device.CreateSampler(&sd);
  const auto usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  post.grade =
      make_texture(device, "post.grade", kGradeSize, kGradeSize, kGradeSize, kHdrFormat, usage);
  post.grade_view = post.grade.CreateView();
  set_grade(device, post, identity_grade());
  post.black = make_texture(device, "post.black", 1, 1, 1, kHdrFormat, usage);
  post.black_view = post.black.CreateView();
  const uint16_t zero[4] = {0, 0, 0, 0x3c00};  // alpha 1: one level, of nothing
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = post.black;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = sizeof zero;
  layout.rowsPerImage = 1;
  const wgpu::Extent3D one{1, 1, 1};
  device.queue.WriteTexture(&dst, zero, sizeof zero, &layout, &one);

  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::post, "post");
  const bool srgb_target = output == wgpu::TextureFormat::RGBA8UnormSrgb ||
                           output == wgpu::TextureFormat::BGRA8UnormSrgb;
  wgpu::ConstantEntry constants[2] = {};
  constants[0].key = "kEncodeSrgb";
  constants[0].value = srgb_target ? 0.0 : 1.0;
  constants[1].key = "kBloom";
  constants[1].value = settings.bloom;
  wgpu::ColorTargetState colour{};
  colour.format = output;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.constantCount = 2;
  fragment.constants = constants;
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

wgpu::BindGroup post_group(gpu::Device& device, const Post& post, const wgpu::TextureView& hdr,
                           const wgpu::TextureView& bloom) {
  return make_group(device, "post", post.layout,
                    {hdr, bloom ? bloom : post.black_view, post.sampler, post.grade_view});
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

std::vector<float> identity_grade() {
  constexpr uint32_t n = kGradeSize;
  std::vector<float> t(size_t{n} * n * n * 3);
  size_t i = 0;
  for (uint32_t b = 0; b < n; ++b) {
    for (uint32_t g = 0; g < n; ++g) {
      for (uint32_t r = 0; r < n; ++r) {
        t[i++] = static_cast<float>(r) / (n - 1);
        t[i++] = static_cast<float>(g) / (n - 1);
        t[i++] = static_cast<float>(b) / (n - 1);
      }
    }
  }
  return t;
}

void set_grade(gpu::Device& device, Post& post, std::span<const float> table) {
  constexpr uint32_t n = kGradeSize;
  const size_t texels = size_t{n} * n * n;
  if (table.size() != texels * 3) {
    core::output::error("render: set_grade was handed " + std::to_string(table.size()) +
                        " floats, not kGradeSize^3 * 3 = " + std::to_string(texels * 3) +
                        "; the grade is unchanged. next: build the table in identity_grade's "
                        "layout (render/post.hpp); skill: render.");
    return;
  }
  std::vector<uint16_t> halves(texels * 4);
  for (size_t k = 0; k < texels; ++k) {
    for (size_t c = 0; c < 3; ++c) halves[k * 4 + c] = float_to_half(table[k * 3 + c]);
    halves[k * 4 + 3] = 0x3c00;
  }
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = post.grade;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = n * 8;
  layout.rowsPerImage = n;
  const wgpu::Extent3D size{n, n, n};
  device.queue.WriteTexture(&dst, halves.data(), halves.size() * 2, &layout, &size);
}

}  // namespace ge::render
