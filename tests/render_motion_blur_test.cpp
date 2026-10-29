// engine/render/motion_blur.hpp: the tiles are the motion target over kTile, rounded up; on a
// device, one moving texel makes its own tile the fastest and reaches the eight tiles round it in
// the neighbours' target, and no further; a still target makes still tiles.
//
// ge:summary Tests for motion blur's tiles: their size, a tile's fastest and its neighbours'
// ge:keywords test render motion-blur tiles tile-max neighbour-max
// ge:skill render testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/motion_blur.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;  // 6 x 4 tiles
constexpr uint16_t kTenth = 0x2e66;   // 0.1 as a half float (0.0999755859375)

// The motion target all still but one texel moving `x` (a half float's bits) in tile (2, 1); the
// two tile targets' red back.
void run(ge::gpu::Device& d, uint16_t x, std::vector<float>& tiles, std::vector<float>& near) {
  ge::gpu::PipelineSet pipelines;
  r::MotionBlur blur;
  r::create_motion_blur_pipelines(d, pipelines, blur);
  CHECK(pipelines.finish(d));
  ge::gpu::end_load(d);
  const wgpu::Texture motion =
      r::make_texture(d, "test.motion", kW, kH, 1, r::kHdrFormat,
                      wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  std::vector<uint16_t> texels(size_t{kW} * kH * 4, 0);
  const size_t at = (size_t{1} * r::kTile + 5) * kW + (2 * r::kTile + 9);
  texels[at * 4] = x;
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = motion;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kW * 8;
  layout.rowsPerImage = kH;
  const wgpu::Extent3D size{kW, kH, 1};
  d.queue.WriteTexture(&dst, texels.data(), texels.size() * 2, &layout, &size);
  r::create_motion_blur_targets(d, blur, kW, kH, motion.CreateView());
  r::FrameRecorder rec;
  rec.init(d, "test.timer", 2);
  rec.begin_frame(d);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_motion_blur(blur, pipelines, rec, e);
  rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  rec.after_submit(d);
  tiles = r::read_red(d, blur.tiles, 0, blur.width, blur.height, r::kHdrFormat);
  near = r::read_red(d, blur.near, 0, blur.width, blur.height, r::kHdrFormat);
}

}  // namespace

TEST(the_tiles_are_the_target_over_the_tile_rounded_up) {
  uint32_t w, h;
  r::motion_blur_size(1280, 720, &w, &h);
  CHECK(w == 80 && h == 45);
  r::motion_blur_size(1281, 1, &w, &h);
  CHECK(w == 81 && h == 1);
}

TEST(one_moving_texel_fills_its_tile_and_reaches_its_neighbours_only) {
  auto device = ge::test::test_device();
  if (!device) return;
  std::vector<float> tiles, near;
  run(*device, kTenth, tiles, near);
  CHECK(tiles.size() == 24 && near.size() == 24);
  for (size_t ty = 0; ty < 4; ++ty) {
    for (size_t tx = 0; tx < 6; ++tx) {
      const float own = tiles[ty * 6 + tx];
      const float round = near[ty * 6 + tx];
      const bool moving = tx == 2 && ty == 1;
      const bool beside = tx >= 1 && tx <= 3 && ty <= 2;
      CHECK(moving ? own > 0.09f : own == 0.0f);
      CHECK(beside ? round > 0.09f : round == 0.0f);
    }
  }
  CHECK(device->errors.load() == 0);
  // A fresh device: its pipelines are all made before its first frame.
  auto still = ge::test::test_device();
  run(*still, 0, tiles, near);
  for (float v : near) CHECK(v == 0.0f);
  CHECK(still->errors.load() == 0);
}
