// engine/render/post.hpp and bloom.hpp: halves round to the nearest; the bloom's chain halves
// down to its levels; on a device, the bloom spreads a bright pixel's light round it while a
// field of one grey keeps its grey (the mix conserves energy), and the grade's table is read in
// its documented layout (a table that swaps red and blue swaps them in the picture); the lens and
// the film leave a flat field flat (the sharpening and the fringes), darken only the corners (the
// vignette) and add grain that averages out.
//
// ge:summary Tests for the post pass: halves, the bloom's chain and glow, the grade's table
// ge:keywords test render post bloom grade lut float_to_half energy
// ge:skill render testing
#include <cmath>
#include <cstdlib>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/bloom.hpp"
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
  wgpu::BindGroup frame;
  r::Sky sky;
  r::Bloom bloom;
  r::Post plain, glow, crisp, vignetted, grainy;  // crisp: sharpened and fringed
  wgpu::Texture hdr;
  wgpu::TextureView hdr_view;
  r::FrameRecorder rec;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::BloomSettings bs;
  bs.strength = 0.5;
  r::create_bloom_pipelines(d, s.pipelines, s.bloom, bs);
  r::create_post(d, s.pipelines, s.plain, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  r::PostSettings ps;
  ps.bloom = 0.5;
  r::create_post(d, s.pipelines, s.glow, s.layout, wgpu::TextureFormat::RGBA8Unorm, ps);
  const auto fmt = wgpu::TextureFormat::RGBA8Unorm;
  r::create_post(d, s.pipelines, s.crisp, s.layout, fmt, {.look = {.sharpen = 1, .chromatic = 1}});
  r::create_post(d, s.pipelines, s.vignetted, s.layout, fmt, {.look = {.vignette = 1}});
  r::create_post(d, s.pipelines, s.grainy, s.layout, fmt, {.look = {.grain = 1}});
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.hdr = r::make_texture(d, "test.hdr", kW, kH, 1, r::kHdrFormat,
                          wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  s.hdr_view = s.hdr.CreateView();
  const wgpu::TextureView inputs[2] = {s.hdr_view, s.hdr_view};
  r::create_bloom_targets(d, s.bloom, kW, kH, inputs);
  s.rec.init(d, "test.timer", 4);
}

// The HDR frame: every texel `grey` (linear, in units of the sky's exposure) and, when `bright`
// is above zero, the centre texel that bright.
void fill(ge::gpu::Device& d, Scene& s, const float rgb[3], float bright) {
  std::vector<uint16_t> texels(size_t{kW} * kH * 4);
  for (size_t k = 0; k < size_t{kW} * kH; ++k) {
    for (size_t c = 0; c < 3; ++c) texels[k * 4 + c] = r::float_to_half(rgb[c]);
    texels[k * 4 + 3] = r::float_to_half(1.0f);
  }
  if (bright > 0) {
    const size_t k = size_t{kH / 2} * kW + kW / 2;
    for (size_t c = 0; c < 3; ++c) texels[k * 4 + c] = r::float_to_half(bright);
  }
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = s.hdr;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kW * 8;
  layout.rowsPerImage = kH;
  const wgpu::Extent3D size{kW, kH, 1};
  d.queue.WriteTexture(&dst, texels.data(), texels.size() * 2, &layout, &size);
}

ge::gpu::Image draw(ge::gpu::Device& d, Scene& s, const r::Post& post, bool bloom,
                    const ge::gpu::Target& out) {
  r::FrameView v;
  v.sun_elevation = 0.6;
  v.altitude_m = 1000;
  s.rec.begin_frame(d);
  r::fill_uniforms(s.staged, v, kW, kH, nullptr);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  const wgpu::BindGroup group = r::post_group(d, post, s.hdr_view, bloom ? s.bloom.view : nullptr);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  if (bloom) r::encode_bloom(s.bloom, s.pipelines, s.rec, e, 0);
  r::encode_post(post, s.pipelines, s.rec, e, s.frame, group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

int at(const ge::gpu::Image& img, uint32_t x, uint32_t y, int c) {
  return img.rgba[(size_t{y} * img.width + x) * 4 + static_cast<size_t>(c)];
}

}  // namespace

TEST(halves_round_to_the_nearest) {
  CHECK(r::float_to_half(1.0f) == 0x3c00);
  CHECK(r::float_to_half(-2.0f) == 0xc000);
  CHECK(r::float_to_half(65504.0f) == 0x7bff);
  CHECK(r::float_to_half(1e6f) == 0x7c00);
  CHECK(r::float_to_half(0.0f) == 0);
  CHECK(r::float_to_half(5.960464477539063e-8f) == 1);  // the smallest subnormal
  // Every half survives the round trip, and a float between two halves lands on the nearer.
  for (uint32_t h = 0; h < 0x7c00; ++h) {
    CHECK(r::float_to_half(r::half_to_float(static_cast<uint16_t>(h))) == h);
  }
  CHECK(r::float_to_half(1.0f + 0.4f / 1024) == 0x3c00);
  CHECK(r::float_to_half(1.0f + 0.6f / 1024) == 0x3c01);
}

TEST(the_bloom_chain_halves_to_its_levels) {
  uint32_t w, h;
  int n;
  r::bloom_size(640, 360, 6, &w, &h, &n);
  CHECK(w == 320 && h == 180 && n == 6);  // down to 10 x 5
  r::bloom_size(8, 8, 6, &w, &h, &n);
  CHECK(w == 4 && h == 4 && n == 2);  // 4 x 4, then 2 x 2
  r::bloom_size(1, 1, 6, &w, &h, &n);
  CHECK(w == 1 && h == 1 && n == 1);
}

TEST(bloom_spreads_light_and_keeps_a_flat_field) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const float grey[3] = {0.1f, 0.1f, 0.1f};
  fill(d, s, grey, 0);
  const ge::gpu::Image flat = draw(d, s, s.plain, false, out);
  const ge::gpu::Image flat_glow = draw(d, s, s.glow, true, out);
  int worst = 0;
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      worst = std::max(worst, std::abs(at(flat, x, y, 1) - at(flat_glow, x, y, 1)));
    }
  }
  CHECK(worst <= 1);  // a flat field's glow is the field
  fill(d, s, grey, 2000.0f);
  const ge::gpu::Image plain = draw(d, s, s.plain, false, out);
  const ge::gpu::Image glow = draw(d, s, s.glow, true, out);
  // Round the bright texel the glow lifts its neighbours, less the farther they are.
  const int near = at(glow, kW / 2 + 3, kH / 2, 1) - at(plain, kW / 2 + 3, kH / 2, 1);
  const int far = at(glow, kW / 2 + 12, kH / 2, 1) - at(plain, kW / 2 + 12, kH / 2, 1);
  CHECK(near > 10);
  CHECK(far > 0 && far < near);
  CHECK(at(plain, kW / 2 + 3, kH / 2, 1) == at(flat, kW / 2 + 3, kH / 2, 1));  // no glow without
  CHECK(d.errors.load() == 0);
}

TEST(the_grade_is_read_in_its_layout) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const float colour[3] = {0.3f, 0.1f, 0.02f};
  fill(d, s, colour, 0);
  const ge::gpu::Image same = draw(d, s, s.plain, false, out);
  std::vector<float> swap = r::identity_grade();
  for (size_t k = 0; k < swap.size(); k += 3) std::swap(swap[k], swap[k + 2]);
  r::set_grade(d, s.plain, swap);
  const ge::gpu::Image swapped = draw(d, s, s.plain, false, out);
  CHECK(at(same, 5, 5, 0) > at(same, 5, 5, 2) + 40);  // orange
  CHECK(std::abs(at(swapped, 5, 5, 0) - at(same, 5, 5, 2)) <= 1);
  CHECK(std::abs(at(swapped, 5, 5, 2) - at(same, 5, 5, 0)) <= 1);
  CHECK(std::abs(at(swapped, 5, 5, 1) - at(same, 5, 5, 1)) <= 1);
  r::set_grade(d, s.plain, std::vector<float>(3));  // the wrong size: an error line, no change
  const ge::gpu::Image kept = draw(d, s, s.plain, false, out);
  CHECK(at(kept, 5, 5, 0) == at(swapped, 5, 5, 0));
  CHECK(d.errors.load() == 0);
}

TEST(the_lens_and_the_film_do_what_they_name) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const float grey[3] = {0.1f, 0.1f, 0.1f};
  fill(d, s, grey, 0);
  const ge::gpu::Image plain = draw(d, s, s.plain, false, out);
  const ge::gpu::Image crisp = draw(d, s, s.crisp, false, out);
  const ge::gpu::Image dark = draw(d, s, s.vignetted, false, out);
  const ge::gpu::Image grain = draw(d, s, s.grainy, false, out);
  int worst = 0, changed = 0;
  long sum = 0, grain_sum = 0;
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      worst = std::max(worst, std::abs(at(plain, x, y, 1) - at(crisp, x, y, 1)));
      changed += at(plain, x, y, 1) != at(grain, x, y, 1) ? 1 : 0;
      sum += at(plain, x, y, 1);
      grain_sum += at(grain, x, y, 1);
    }
  }
  CHECK(worst <= 1);  // nothing to sharpen or fringe in a flat field
  CHECK(std::abs(at(dark, kW / 2, kH / 2, 1) - at(plain, kW / 2, kH / 2, 1)) <= 1);
  CHECK(at(dark, 0, 0, 1) + 10 < at(plain, 0, 0, 1));              // the corner darker
  CHECK(changed > static_cast<int>(kW * kH / 4));                  // grain on most pixels
  CHECK(std::labs(grain_sum - sum) < static_cast<long>(kW * kH));  // averaging under a level
  CHECK(d.errors.load() == 0);
}
