// engine/render/lens.hpp: the target is a quarter of the output, rounded up; on a device, with
// the sun in the middle of the picture over open sky the rays light the target round it and the
// flare lights it too; with the depth target full (something stands in front of everything) both
// are dark; with the sun behind the eye both are dark; and a frame with neither half makes none.
//
// ge:summary Tests for the lens pass: its size, rays and flare over open sky, blocked, behind
// ge:keywords test render lens sun-rays flare occlusion quarter-resolution
// ge:skill render testing
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/lens.hpp"
#include "render/sky.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

struct Scene {
  ge::gpu::PipelineSet pipelines;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame;
  r::Sky sky;
  r::Lens rays, flare;
  r::FrameTargets targets;
  r::FrameRecorder rec;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::create_lens_pipelines(d, s.pipelines, s.rays, s.layout, {.rays = 1});
  r::create_lens_pipelines(d, s.pipelines, s.flare, s.layout, {.flare = 1});
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.targets = r::create_targets(d, "test", kW, kH);
  const wgpu::TextureView inputs[2] = {s.targets.hdr_view, s.targets.hdr_view};
  r::create_lens_targets(d, s.rays, kW, kH, inputs, s.targets.depth_view);
  r::create_lens_targets(d, s.flare, kW, kH, inputs, s.targets.depth_view);
  s.rec.init(d, "test.timer", 4);
}

// The HDR target a flat bright sky (`sky` in the sky's exposed units), the depth cleared to
// `depth` (0: nothing drawn, 1: something at the near plane everywhere), the lens drawn with the
// camera looking `away` from the sun or at it; the lens target's red channel back.
std::vector<float> draw(ge::gpu::Device& d, Scene& s, const r::Lens& lens, float depth, bool away) {
  r::FrameView v;
  v.sun_elevation = 0.3;
  v.sun_azimuth = 1.0;
  v.altitude_m = 1000;
  v.heading = away ? 1.0 + 3.14159265 : 1.0;
  v.pitch = away ? 0.0 : 0.3;
  s.rec.begin_frame(d);
  r::fill_uniforms(s.staged, v, kW, kH, nullptr);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  {
    wgpu::RenderPassColorAttachment colour{};
    colour.view = s.targets.hdr_view;
    colour.loadOp = wgpu::LoadOp::Clear;
    colour.storeOp = wgpu::StoreOp::Store;
    colour.clearValue = {50, 50, 50, 1};  // far brighter than white: an open sky near the sun
    wgpu::RenderPassDepthStencilAttachment z{};
    z.view = s.targets.depth_view;
    z.depthLoadOp = wgpu::LoadOp::Clear;
    z.depthStoreOp = wgpu::StoreOp::Store;
    z.depthClearValue = depth;
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &colour;
    desc.depthStencilAttachment = &z;
    wgpu::RenderPassEncoder pass = s.rec.render(e, desc, "clear");
    pass.End();
  }
  r::encode_lens(lens, s.pipelines, s.rec, e, s.frame, 0);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return r::read_red(d, lens.target, 0, lens.width, lens.height, r::kHdrFormat);
}

float most(const std::vector<float>& v) {
  float m = 0;
  for (float x : v) m = std::max(m, std::isfinite(x) ? x : 1e9f);
  return m;
}

}  // namespace

TEST(the_target_is_a_quarter_rounded_up) {
  uint32_t w, h;
  r::lens_size(1280, 720, &w, &h);
  CHECK(w == 320 && h == 180);
  r::lens_size(1281, 2, &w, &h);
  CHECK(w == 321 && h == 1);
  CHECK(!r::lens_on({}) && r::lens_on({.rays = 0.1}) && r::lens_on({.flare = 0.1}));
}

TEST(rays_and_flare_light_open_sky_and_go_dark_when_blocked_or_behind) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const std::vector<float> open = draw(d, s, s.rays, 0.0f, false);
  CHECK(most(open) > 0.0f);
  // Round the sun (the middle, raised by the pitch to the sun's elevation) the rays are lit.
  CHECK(open[(s.rays.height / 2) * s.rays.width + s.rays.width / 2] > 0.0f);
  CHECK(most(draw(d, s, s.rays, 1.0f, false)) == 0.0f);  // a wall in front of the sun
  CHECK(most(draw(d, s, s.rays, 0.0f, true)) == 0.0f);   // the sun behind the eye
  CHECK(most(draw(d, s, s.flare, 0.0f, false)) > 0.0f);
  CHECK(most(draw(d, s, s.flare, 1.0f, false)) == 0.0f);
  CHECK(most(draw(d, s, s.flare, 0.0f, true)) == 0.0f);
  CHECK(d.errors.load() == 0);
}
