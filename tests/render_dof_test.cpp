// engine/render/dof.hpp: the target is half the output, rounded up; on a device, over a checker
// of 2-pixel squares, a wall in focus keeps every square (and a circle of 0), and the open sky far
// past the focus is gathered toward the checker's mean (its circle the widest).
//
// ge:summary Tests for depth of field: its size, a wall in focus kept sharp, the far sky blurred
// ge:keywords test render depth-of-field dof focus circle-of-confusion half-size
// ge:skill render testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu_support.hpp"
#include "render/dof.hpp"
#include "render/frame.hpp"
#include "render/sky.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 384, kH = 256;  // the widest circle 3.8 pixels

struct Scene {
  ge::gpu::PipelineSet pipelines;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame;
  r::Sky sky;
  r::Dof dof;
  r::FrameTargets targets;
  wgpu::Texture checker;
  r::FrameRecorder rec;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::create_dof_pipelines(d, s.pipelines, s.dof, s.layout);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.targets = r::create_targets(d, "test", kW, kH);
  // The input: 1 and 0 in squares of 2 x 2 pixels.
  s.checker = r::make_texture(d, "test.checker", kW, kH, 1, r::kHdrFormat,
                              wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  std::vector<uint16_t> texels(size_t{kW} * kH * 4);
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      const uint16_t v = ((x / 2 + y / 2) % 2 == 0) ? r::float_to_half(1.0f) : uint16_t{0};
      for (uint32_t c = 0; c < 3; ++c) texels[(size_t{y} * kW + x) * 4 + c] = v;
      texels[(size_t{y} * kW + x) * 4 + 3] = r::float_to_half(1.0f);
    }
  }
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = s.checker;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kW * 8;
  layout.rowsPerImage = kH;
  const wgpu::Extent3D size{kW, kH, 1};
  d.queue.WriteTexture(&dst, texels.data(), texels.size() * 2, &layout, &size);
  const wgpu::TextureView inputs[2] = {s.checker.CreateView(), s.checker.CreateView()};
  r::create_dof_targets(d, s.dof, kW, kH, inputs, s.targets.depth_view);
  s.rec.init(d, "test.timer", 2);
}

// The depth cleared to `depth` (0: nothing drawn; 0.01: a wall 10 m out), the gather focused at
// `focus` m; the target's red (the light) and alpha (the circle) back.
void run(ge::gpu::Device& d, Scene& s, float depth, double focus, std::vector<float>& light,
         std::vector<float>& circle) {
  r::FrameView v;
  v.altitude_m = 1000;
  r::fill_uniforms(s.staged, v, kW, kH, nullptr);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::set_dof(d, s.dof, {.strength = 1, .focus_m = focus, .sharp = 0.3});
  s.rec.begin_frame(d);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  {
    wgpu::RenderPassDepthStencilAttachment z{};
    z.view = s.targets.depth_view;
    z.depthLoadOp = wgpu::LoadOp::Clear;
    z.depthStoreOp = wgpu::StoreOp::Store;
    z.depthClearValue = depth;
    wgpu::RenderPassDescriptor desc{};
    desc.depthStencilAttachment = &z;
    s.rec.render(e, desc, "clear").End();
  }
  r::encode_dof(s.dof, s.pipelines, s.rec, e, s.frame, 0);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  light = r::read_red(d, s.dof.target, 0, s.dof.width, s.dof.height, r::kHdrFormat);
  circle = r::read_red(d, s.dof.target, 0, s.dof.width, s.dof.height, r::kHdrFormat, 3);
}

// How far the light strays from the checker's mean, on average, away from the edges.
float spread(const std::vector<float>& v, const Scene& s) {
  double sum = 0;
  int n = 0;
  for (uint32_t y = 8; y + 8 < s.dof.height; ++y) {
    for (uint32_t x = 8; x + 8 < s.dof.width; ++x) {
      sum += std::fabs(v[size_t{y} * s.dof.width + x] - 0.5);
      ++n;
    }
  }
  return static_cast<float>(sum / n);
}

}  // namespace

TEST(the_target_is_half_the_output_rounded_up) {
  uint32_t w, h;
  r::dof_size(1280, 720, &w, &h);
  CHECK(w == 640 && h == 360);
  r::dof_size(1281, 1, &w, &h);
  CHECK(w == 641 && h == 1);
  CHECK(!r::dof_on({}) && r::dof_on({.strength = 0.1}));
}

TEST(a_wall_in_focus_stays_sharp_and_the_far_sky_blurs) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  std::vector<float> light, circle;
  run(d, s, 0.01f, 10, light, circle);
  CHECK(spread(light, s) > 0.49f);  // every half-size texel one square: 1 or 0
  for (float c : circle) CHECK(c == 0.0f);
  run(d, s, 0.0f, 10, light, circle);
  CHECK(spread(light, s) < 0.2f);
  for (float c : circle) CHECK(c == 1.0f);
  CHECK(d.errors.load() == 0);
}
