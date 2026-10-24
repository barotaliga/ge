// The post pass's pipeline, group and draw, and the grade's table.
//
// ge:summary The post pass's pipeline, draw and the grade's lookup table upload
// ge:keywords render post pipeline srgb override grade lut upload half-float lens-dirt
// ge:keywords set_post_look lens_dirt smudges
// ge:skill render
// ge:test tests/render_post_test.cpp
#include "render/post.hpp"

#include <algorithm>
#include <string>

#include "core/output.hpp"
#include "core/prng.hpp"

#include "render/post.wgsl.hpp"

namespace ge::render {

namespace {

constexpr uint32_t kDirtW = 480, kDirtH = 270;  // the dirt's picture: soft, so small

// The strengths as the shader reads them: vec4f(sharpen, chromatic, vignette, grain),
// vec4f(dirt, 0, 0, 0).
struct LookBlock {
  float a[4];
  float b[4];
};

}  // namespace

void create_post(gpu::Device& device, gpu::PipelineSet& pipelines, Post& post,
                 const wgpu::BindGroupLayout& frame, wgpu::TextureFormat output,
                 const PostSettings& settings) {
  post.settings = settings;
  post.output = output;
  post.layout = make_layout(device, "post",
                            {Slot::texture, Slot::texture, Slot::sampler, Slot::texture_3d,
                             Slot::texture, Slot::texture, Slot::uniform},
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
  post.dirt_view = post.black_view;
  if (settings.look.dirt > 0) {
    post.dirt = make_texture(device, "post.dirt", kDirtW, kDirtH, 1,
                             wgpu::TextureFormat::RGBA8Unorm, usage);
    post.dirt_view = post.dirt.CreateView();
    const std::vector<uint8_t> dirt = lens_dirt(kDirtW, kDirtH);
    dst.texture = post.dirt;
    layout.bytesPerRow = kDirtW * 4;
    layout.rowsPerImage = kDirtH;
    const wgpu::Extent3D size{kDirtW, kDirtH, 1};
    device.queue.WriteTexture(&dst, dirt.data(), dirt.size(), &layout, &size);
  }
  post.look = make_buffer(device, "post.look", sizeof(LookBlock), wgpu::BufferUsage::Uniform);
  set_post_look(device, post, settings.look);

  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::post, "post");
  const bool srgb_target = output == wgpu::TextureFormat::RGBA8UnormSrgb ||
                           output == wgpu::TextureFormat::BGRA8UnormSrgb;
  const PostLook& look = settings.look;
  wgpu::ConstantEntry constants[8] = {};
  const std::pair<const char*, double> values[8] = {
      {"kEncodeSrgb", srgb_target ? 0.0 : 1.0},     {"kBloom", settings.bloom},
      {"kSharpen", look.sharpen > 0 ? 1.0 : 0.0},   {"kChromatic", look.chromatic > 0 ? 1.0 : 0.0},
      {"kVignette", look.vignette > 0 ? 1.0 : 0.0}, {"kGrain", look.grain > 0 ? 1.0 : 0.0},
      {"kDirt", look.dirt > 0 ? 1.0 : 0.0},         {"kLens", settings.lens ? 1.0 : 0.0},
  };
  for (size_t k = 0; k < std::size(values); ++k) {
    constants[k].key = values[k].first;
    constants[k].value = values[k].second;
  }
  wgpu::ColorTargetState colour{};
  colour.format = output;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.constantCount = std::size(constants);
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
                           const wgpu::TextureView& bloom, const wgpu::TextureView& lens) {
  return make_group(device, "post", post.layout,
                    {hdr, bloom ? bloom : post.black_view, post.sampler, post.grade_view,
                     lens ? lens : post.black_view, post.dirt_view, post.look});
}

void set_post_look(gpu::Device& device, const Post& post, const PostLook& look) {
  const auto f = [](double v) { return static_cast<float>(std::clamp(v, 0.0, 1.0)); };
  const LookBlock block{{f(look.sharpen), f(look.chromatic), f(look.vignette), f(look.grain)},
                        {f(look.dirt), 0, 0, 0}};
  device.queue.WriteBuffer(post.look, 0, &block, sizeof block);
}

std::vector<uint8_t> lens_dirt(uint32_t w, uint32_t h, uint32_t seed) {
  // Each mark is splatted as a weight with no transcendental (a smooth bump, a ring, a stroke),
  // so the picture is the same bits wherever it is made.
  std::vector<float> amount(size_t{w} * h, 0.0f);
  core::Rng rng = core::make_rng(seed);
  const auto fw = static_cast<float>(w), fh = static_cast<float>(h);
  auto splat = [&](float cx, float cy, float radius, float weight, bool ring) {
    const int x0 = std::max(0, static_cast<int>(cx - radius)),
              x1 = std::min(static_cast<int>(w) - 1, static_cast<int>(cx + radius));
    const int y0 = std::max(0, static_cast<int>(cy - radius)),
              y1 = std::min(static_cast<int>(h) - 1, static_cast<int>(cy + radius));
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float dx = (static_cast<float>(x) + 0.5f - cx) / radius,
                    dy = (static_cast<float>(y) + 0.5f - cy) / radius;
        const float d2 = dx * dx + dy * dy;
        if (d2 >= 1) continue;
        float v = (1 - d2) * (1 - d2);  // a soft bump
        if (ring) {
          const float e = (d2 - 0.75f) * 4;  // a band near the edge, as a dried drop leaves
          v = std::max(0.0f, 1 - e * e) * 0.8f + v * 0.2f;
        }
        amount[static_cast<size_t>(y) * w + static_cast<size_t>(x)] += v * weight;
      }
    }
  };
  for (int i = 0; i < 70; ++i) {  // the smudges and the dried drops
    const auto cx = static_cast<float>(core::range(rng, 0, fw));
    const auto cy = static_cast<float>(core::range(rng, 0, fh));
    const auto big = static_cast<float>(core::next(rng));
    const float radius = fh * (0.015f + 0.11f * big * big);
    const bool ring = core::next(rng) < 0.3;
    splat(cx, cy, radius, static_cast<float>(core::range(rng, 0.15, 0.6)) * (1.1f - big), ring);
  }
  for (int i = 0; i < 5; ++i) {  // a few wiped strokes, faint
    float x = static_cast<float>(core::range(rng, 0, fw));
    float y = static_cast<float>(core::range(rng, 0, fh));
    const auto dx = static_cast<float>(core::range(rng, -1, 1)),
               dy = static_cast<float>(core::range(rng, -0.4, 0.4));
    for (int k = 0; k < 60; ++k) {
      splat(x, y, fh * 0.02f, 0.05f, false);
      x += dx * fh * 0.01f;
      y += dy * fh * 0.01f;
    }
  }
  std::vector<uint8_t> out(amount.size() * 4);
  for (size_t i = 0; i < amount.size(); ++i) {
    const float a = std::min(amount[i], 1.0f);
    // A faintly warm grime: dust scatters a little more red than blue.
    out[i * 4 + 0] = static_cast<uint8_t>(a * 255.0f + 0.5f);
    out[i * 4 + 1] = static_cast<uint8_t>(a * 240.0f + 0.5f);
    out[i * 4 + 2] = static_cast<uint8_t>(a * 220.0f + 0.5f);
    out[i * 4 + 3] = 255;
  }
  return out;
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
