// engine/render/eye.hpp: the meter reads a flat field's luminance at the sky's exposure to a
// bin; a frame never metered and one metered inside the key's band show the same bits; a dark
// field opens the eye to its cap and brightens the picture; the step halves the way in its
// half-life, opening slowly and closing fast; a bright field closes the eye only past min_ev.
//
// ge:summary Tests for the eye's adaptation: the meter, the band, the cap and the half-lives
// ge:keywords test render eye eye-adaptation auto-exposure histogram half-life meter
// ge:skill render testing
#include <cmath>
#include <cstdlib>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/eye.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 64, kH = 64;

struct Scene {
  ge::gpu::PipelineSet pipelines;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group, eye_group;
  r::Sky sky;
  r::Post post;
  r::Eye eye;
  wgpu::Texture hdr;
  wgpu::TextureView hdr_view;
  r::FrameRecorder rec;
};

void make_scene(ge::gpu::Device& d, Scene& s, const r::EyeSettings& settings = {}) {
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  r::create_eye(d, s.pipelines, s.eye, s.layout, s.sky, settings);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.hdr = r::make_texture(d, "test.hdr", kW, kH, 1, r::kHdrFormat,
                          wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  s.hdr_view = s.hdr.CreateView();
  s.post_group = r::post_group(d, s.post, s.hdr_view);
  s.eye_group = r::eye_group(d, s.eye, s.hdr_view);
  s.rec.init(d, "test.timer", 4);
}

// Every texel the grey `g` (linear, the frame's stored light).
void fill(ge::gpu::Device& d, Scene& s, float g) {
  std::vector<uint16_t> texels(size_t{kW} * kH * 4);
  for (size_t k = 0; k < size_t{kW} * kH; ++k) {
    for (size_t c = 0; c < 3; ++c) texels[k * 4 + c] = r::float_to_half(g);
    texels[k * 4 + 3] = r::float_to_half(1.0f);
  }
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = s.hdr;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kW * 8;
  layout.rowsPerImage = kH;
  const wgpu::Extent3D size{kW, kH, 1};
  d.queue.WriteTexture(&dst, texels.data(), texels.size() * 2, &layout, &size);
}

// A frame `dt` s on: the sky's tables, the eye (when `adapt`), the post pass into `out`.
ge::gpu::Image draw(ge::gpu::Device& d, Scene& s, bool adapt, double dt,
                    const ge::gpu::Target& out) {
  r::FrameView v;
  v.sun_elevation = 0.6;
  v.altitude_m = 1000;
  s.rec.begin_frame(d);
  r::fill_uniforms(s.staged, v, kW, kH, nullptr);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  if (adapt) r::prepare_eye(d, s.eye, dt);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  if (adapt) r::encode_eye(s.eye, s.pipelines, s.rec, e, s.frame, s.eye_group);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

int green(const ge::gpu::Image& img) {
  return img.rgba[(size_t{kH / 2} * img.width + kW / 2) * 4 + 1];
}

// The grey whose luminance at the sky's exposure is 2^ev: one metered grey scaled.
float grey_at(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out, double ev) {
  fill(d, s, 0.01f);
  r::cut_eye(s.eye);
  draw(d, s, true, 0, out);
  const double a = r::read_eye(d, s.sky).average;
  return static_cast<float>(0.01 * std::exp2(ev - a));
}

}  // namespace

TEST(the_meter_reads_a_field_to_its_bin) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const r::EyeReading never = r::read_eye(d, s.sky);
  CHECK(never.open_ev == 0 && never.weight == 0);  // a zeroed buffer: the eye at rest
  fill(d, s, 0.01f);
  draw(d, s, true, 0, out);
  const r::EyeReading a = r::read_eye(d, s.sky);
  fill(d, s, 0.08f);
  draw(d, s, true, 0, out);
  const r::EyeReading b = r::read_eye(d, s.sky);
  CHECK(a.weight > 0 && b.weight == a.weight);
  CHECK(std::fabs((b.average - a.average) - 3.0) <= 0.25);  // eight times the light: three stops
  CHECK(d.errors.load() == 0);
}

TEST(inside_the_band_the_eye_keeps_the_bits) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const float key = grey_at(d, s, out, std::log2(0.18));
  auto other = ge::test::test_device();
  ge::gpu::Device& d2 = *other;
  Scene rest;
  make_scene(d2, rest);
  fill(d2, rest, key);
  const ge::gpu::Target out2 = ge::gpu::create_target(d2, "test.final", kW, kH);
  const ge::gpu::Image unmetered = draw(d2, rest, false, 0, out2);
  fill(d, s, key);
  r::cut_eye(s.eye);
  const ge::gpu::Image metered = draw(d, s, true, 1.0 / 60, out);
  CHECK(r::read_eye(d, s.sky).open_ev == 0);
  CHECK(unmetered.rgba == metered.rgba);
  CHECK(d.errors.load() == 0);
}

TEST(a_dark_field_opens_the_eye_to_its_cap) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  r::EyeSettings es;
  es.strength = 1;
  Scene s;
  make_scene(d, s, es);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  // Two stops under the band's foot: the eye opens two stops.
  const float dim = grey_at(d, s, out, std::log2(0.18) - es.tolerance_ev - 2);
  fill(d, s, dim);
  const ge::gpu::Image rest = draw(d, s, false, 0, out);
  r::cut_eye(s.eye);
  const ge::gpu::Image open = draw(d, s, true, 0, out);
  const r::EyeReading e = r::read_eye(d, s.sky);
  CHECK(std::fabs(e.open_ev - 2.0) <= 0.25);
  CHECK(green(open) > green(rest) + 10);
  // Six stops under: held at max_ev.
  fill(d, s, dim / 16);
  r::cut_eye(s.eye);
  draw(d, s, true, 0, out);
  CHECK(std::fabs(r::read_eye(d, s.sky).open_ev - es.max_ev) <= 1e-6);
  CHECK(d.errors.load() == 0);
}

TEST(the_eye_opens_slowly_and_closes_fast) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const r::EyeSettings& es = s.eye.settings;
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const float bright = grey_at(d, s, out, std::log2(0.18));
  fill(d, s, bright);
  r::cut_eye(s.eye);
  draw(d, s, true, 0, out);   // at rest
  fill(d, s, bright / 4096);  // far under: the target is the cap
  draw(d, s, true, es.to_dark_s, out);
  const r::EyeReading half = r::read_eye(d, s.sky);
  CHECK(std::fabs(half.target_ev - es.max_ev) <= 1e-6);
  CHECK(std::fabs(half.open_ev - es.max_ev / 2) <= 1e-3);  // one half-life: half the way
  r::cut_eye(s.eye);
  draw(d, s, true, 0, out);
  fill(d, s, bright);
  draw(d, s, true, es.to_light_s, out);
  CHECK(std::fabs(r::read_eye(d, s.sky).open_ev - es.max_ev / 2) <= 1e-3);
  draw(d, s, true, es.to_light_s * 4, out);
  CHECK(r::read_eye(d, s.sky).open_ev < es.max_ev / 16);
  CHECK(d.errors.load() == 0);
}

TEST(a_bright_field_closes_the_eye_only_past_min_ev) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  fill(d, s, grey_at(d, s, out, std::log2(0.18) + 4));
  r::cut_eye(s.eye);
  draw(d, s, true, 0, out);
  CHECK(r::read_eye(d, s.sky).open_ev == 0);  // min_ev 0: the sky's exposure is the floor
  s.eye.settings.min_ev = -2;
  s.eye.settings.strength = 1;
  r::cut_eye(s.eye);
  draw(d, s, true, 0, out);
  CHECK(std::fabs(r::read_eye(d, s.sky).open_ev + 2.0) <= 1e-6);  // three stops over, held at -2
  CHECK(d.errors.load() == 0);
}
