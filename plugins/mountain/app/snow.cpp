// The snow's ground map, its bind group and its shader.
//
// ge:summary The snow material's ground map bake, bind group and shader
// ge:keywords snow ground-map bake piste-direction doubled-angle
// ge:skill mountain render
#include "mountain/app/snow.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "mountain/app/snow.wgsl.hpp"
#include "render/frame.hpp"

namespace ge::mountain::app {

namespace {

// Snow holds below this slope (rise over run) and is gone above the second.
constexpr double kHolds = 1.0;  // 45 degrees
constexpr double kBare = 1.6;   // about 58 degrees
// The direction fades in over this far inside the piste's edge, m.
constexpr double kEdgeFade = 4.0;

uint8_t unorm(double v) {
  return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

struct Params {
  float look[4];
  float look2[4];
};

Params params_of(const SnowLook& l) {
  return {{l.glitter, l.corduroy, l.relief, l.wrap}, {l.glare, l.lanes, l.tracks, 0}};
}

}  // namespace

double rock_share(double slope) {
  const double t = std::clamp((slope - kHolds) / (kBare - kHolds), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

std::vector<uint8_t> bake_snow_ground(const mapgen::Level& level) {
  const auto& g = level.ground;
  const size_t cols = static_cast<size_t>(g.cols), rows = static_cast<size_t>(g.rows);
  std::vector<uint8_t> out(cols * rows * 4);
  const bool packed = g.cols == level.packed.cols && g.rows == level.packed.rows;
  auto at = [&](int c, int r) {
    c = std::clamp(c, 0, g.cols - 1);
    r = std::clamp(r, 0, g.rows - 1);
    return static_cast<double>(g.data[static_cast<size_t>(r) * cols + static_cast<size_t>(c)]);
  };
  for (int r = 0; r < g.rows; ++r) {
    for (int c = 0; c < g.cols; ++c) {
      const size_t i = static_cast<size_t>(r) * cols + static_cast<size_t>(c);
      const double gx = (at(c + 1, r) - at(c - 1, r)) / (2 * g.cell);
      const double gz = (at(c, r + 1) - at(c, r - 1)) / (2 * g.cell);
      out[i * 4] = unorm(packed ? level.packed.data[i] : 0.0);
      out[i * 4 + 1] = unorm(rock_share(std::sqrt(gx * gx + gz * gz)));
      out[i * 4 + 2] = 128;
      out[i * 4 + 3] = 128;
    }
  }
  // The direction: each sample takes the nearest piste point's heading within its half width.
  std::vector<double> best(cols * rows, std::numeric_limits<double>::infinity());
  for (const mapgen::TrackPoint& p : level.track.points) {
    const double reach = p.width / 2;
    const int c0 = std::max(0, static_cast<int>(std::floor((p.x - reach - g.origin_x) / g.cell)));
    const int c1 =
        std::min(g.cols - 1, static_cast<int>(std::ceil((p.x + reach - g.origin_x) / g.cell)));
    const int r0 = std::max(0, static_cast<int>(std::floor((p.z - reach - g.origin_z) / g.cell)));
    const int r1 =
        std::min(g.rows - 1, static_cast<int>(std::ceil((p.z + reach - g.origin_z) / g.cell)));
    const double dx = std::cos(2 * p.heading), dz = std::sin(2 * p.heading);
    for (int r = r0; r <= r1; ++r) {
      for (int c = c0; c <= c1; ++c) {
        const double x = g.origin_x + c * g.cell, z = g.origin_z + r * g.cell;
        const double d = std::hypot(x - p.x, z - p.z);
        const size_t i = static_cast<size_t>(r) * cols + static_cast<size_t>(c);
        if (d > reach || d >= best[i]) continue;
        best[i] = d;
        const double k = std::clamp((reach - d) / kEdgeFade, 0.0, 1.0);
        out[i * 4 + 2] = unorm(0.5 + 0.5 * dx * k);
        out[i * 4 + 3] = unorm(0.5 + 0.5 * dz * k);
      }
    }
  }
  return out;
}

void create_snow(gpu::Device& device, SnowMaterial& s, const mapgen::Level& level,
                 const SnowLook& look, const Trails* trails) {
  namespace r = ge::render;
  const auto& g = level.ground;
  const uint32_t w = static_cast<uint32_t>(g.cols), h = static_cast<uint32_t>(g.rows);
  const std::vector<uint8_t> ground = bake_snow_ground(level);
  s.ground = r::make_texture(device, "snow.ground", w, h, 1, wgpu::TextureFormat::RGBA8Unorm,
                             wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = s.ground;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = w * 4;
  layout.rowsPerImage = h;
  const wgpu::Extent3D size{w, h, 1};
  device.queue.WriteTexture(&dst, ground.data(), ground.size(), &layout, &size);
  s.params = r::make_buffer(device, "snow.params", sizeof(Params), wgpu::BufferUsage::Uniform);
  set_snow_look(device, s, look);
  s.layout = r::make_layout(device, "snow",
                            {r::Slot::uniform, r::Slot::texture, r::Slot::texture, r::Slot::texture,
                             r::Slot::uniform, r::Slot::uniform},
                            wgpu::ShaderStage::Fragment);
  if (trails) {
    s.group = r::make_group(device, "snow", s.layout,
                            {s.params, s.ground.CreateView(), trails->fine.view,
                             trails->coarse.view, trails->rects[0], trails->rects[1]});
    return;
  }
  // No tracks: a clear texel, and both rects a metre square far off the map.
  s.no_trail = r::make_texture(device, "snow.no_trail", 1, 1, 1, wgpu::TextureFormat::RGBA8Unorm,
                               wgpu::TextureUsage::TextureBinding);
  const TrailRect nowhere{-1e7f, -1e7f, 1, 1};
  for (int k = 0; k < 2; ++k) {
    s.no_rects[k] =
        r::make_buffer(device, "snow.no_trail.rect", sizeof nowhere, wgpu::BufferUsage::Uniform);
    device.queue.WriteBuffer(s.no_rects[k], 0, &nowhere, sizeof nowhere);
  }
  const wgpu::TextureView none = s.no_trail.CreateView();
  s.group =
      r::make_group(device, "snow", s.layout,
                    {s.params, s.ground.CreateView(), none, none, s.no_rects[0], s.no_rects[1]});
}

void set_snow_look(gpu::Device& device, SnowMaterial& s, const SnowLook& look) {
  const Params p = params_of(look);
  device.queue.WriteBuffer(s.params, 0, &p, sizeof p);
}

std::string_view snow_shader() { return ge::wgsl::snow; }

}  // namespace ge::mountain::app
