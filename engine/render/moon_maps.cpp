// Other moons' faces: the embedded PNGs decoded and uploaded with their mips (render/moon_maps.hpp
// says what each map is).
//
// ge:summary Other moons' maps decoded from their embedded PNGs and uploaded as mipmapped arrays
// ge:keywords render moons moon-maps decode png texture-array mipmap upload
// ge:skill render
// ge:test tests/render_moons_test.cpp
#include "render/moon_maps.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

#include "assets/image.hpp"
#include "render/moons.hpp"

// The maps' bytes, compiled in by cmake/EmbedBinary.cmake (CMakeLists.txt's ge_render).
extern const unsigned char ge_moon_map_phobos[];
extern const std::size_t ge_moon_map_phobos_size;
extern const unsigned char ge_moon_map_deimos[];
extern const std::size_t ge_moon_map_deimos_size;
extern const unsigned char ge_moon_map_io[];
extern const std::size_t ge_moon_map_io_size;
extern const unsigned char ge_moon_map_europa[];
extern const std::size_t ge_moon_map_europa_size;
extern const unsigned char ge_moon_map_ganymede[];
extern const std::size_t ge_moon_map_ganymede_size;
extern const unsigned char ge_moon_map_callisto[];
extern const std::size_t ge_moon_map_callisto_size;
extern const unsigned char ge_moon_map_titan[];
extern const std::size_t ge_moon_map_titan_size;
extern const unsigned char ge_moon_map_enceladus[];
extern const std::size_t ge_moon_map_enceladus_size;
extern const unsigned char ge_moon_map_miranda[];
extern const std::size_t ge_moon_map_miranda_size;
extern const unsigned char ge_moon_map_triton[];
extern const std::size_t ge_moon_map_triton_size;
extern const unsigned char ge_moon_colour_io[];
extern const std::size_t ge_moon_colour_io_size;
extern const unsigned char ge_moon_colour_ganymede[];
extern const std::size_t ge_moon_colour_ganymede_size;
extern const unsigned char ge_moon_colour_triton[];
extern const std::size_t ge_moon_colour_triton_size;

namespace ge::render {

namespace {

using Bytes = std::span<const uint8_t>;

constexpr int kKinds = 10;

Bytes map_png(int kind) {
  static const std::array<Bytes, kKinds> maps = {{
      {ge_moon_map_phobos, ge_moon_map_phobos_size},
      {ge_moon_map_deimos, ge_moon_map_deimos_size},
      {ge_moon_map_io, ge_moon_map_io_size},
      {ge_moon_map_europa, ge_moon_map_europa_size},
      {ge_moon_map_ganymede, ge_moon_map_ganymede_size},
      {ge_moon_map_callisto, ge_moon_map_callisto_size},
      {ge_moon_map_titan, ge_moon_map_titan_size},
      {ge_moon_map_enceladus, ge_moon_map_enceladus_size},
      {ge_moon_map_miranda, ge_moon_map_miranda_size},
      {ge_moon_map_triton, ge_moon_map_triton_size},
  }};
  return kind >= 0 && kind < kKinds ? maps[static_cast<size_t>(kind)] : Bytes{};
}

Bytes colour_png(int kind) {
  switch (kind) {
    case 2:
      return {ge_moon_colour_io, ge_moon_colour_io_size};
    case 4:
      return {ge_moon_colour_ganymede, ge_moon_colour_ganymede_size};
    case 9:
      return {ge_moon_colour_triton, ge_moon_colour_triton_size};
    default:
      return {};
  }
}

// The image's first `channels` channels, or nothing when it does not decode at `w` x `h`.
std::vector<uint8_t> decode(Bytes png, int w, int h, int channels) {
  if (png.empty()) return {};
  const std::optional<assets::Image> image = assets::decode_png(png);
  if (!image || image->width != w || image->height != h) return {};
  std::vector<uint8_t> out(static_cast<size_t>(w) * h * channels);
  for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
    for (int c = 0; c < channels; ++c) out[i * channels + c] = image->rgba[i * 4 + c];
  }
  return out;
}

// An array texture of `layers` layers `w` x `h` in `format` (`channels` bytes a texel), each
// layer's bytes from `layer(k)`, with every mip level the box filter of the one above.
template <class Layer>
wgpu::Texture upload_array(gpu::Device& device, const char* label, int w, int h, int layers,
                           wgpu::TextureFormat format, int channels, Layer layer) {
  uint32_t levels = 1;
  while ((static_cast<uint32_t>(h) >> levels) >= 1) ++levels;
  wgpu::TextureDescriptor td{};
  td.label = label;
  td.size = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(layers)};
  td.format = format;
  td.mipLevelCount = levels;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  wgpu::Texture texture = device.device.CreateTexture(&td);
  const auto ch = static_cast<size_t>(channels);
  for (int k = 0; k < layers; ++k) {
    const std::vector<uint8_t> top = layer(k);
    std::vector<double> level(top.begin(), top.end());
    auto lw = static_cast<uint32_t>(w), lh = static_cast<uint32_t>(h);
    for (uint32_t mip = 0; mip < levels; ++mip) {
      // A row is padded to 256 bytes, as a copy's rows must be.
      const size_t row = (lw * ch + 255u) / 256u * 256u;
      std::vector<uint8_t> bytes(row * lh, 0);
      for (uint32_t y = 0; y < lh; ++y) {
        for (size_t x = 0; x < lw * ch; ++x) {
          bytes[y * row + x] = static_cast<uint8_t>(
              std::lround(std::clamp(level[static_cast<size_t>(y) * lw * ch + x], 0.0, 255.0)));
        }
      }
      wgpu::TexelCopyTextureInfo dst{};
      dst.texture = texture;
      dst.mipLevel = mip;
      dst.origin = {0, 0, static_cast<uint32_t>(k)};
      wgpu::TexelCopyBufferLayout copy{};
      copy.bytesPerRow = static_cast<uint32_t>(row);
      copy.rowsPerImage = lh;
      const wgpu::Extent3D size{lw, lh, 1};
      device.queue.WriteTexture(&dst, bytes.data(), bytes.size(), &copy, &size);
      if (mip + 1 == levels) break;
      const uint32_t nw = std::max(1u, lw / 2), nh = std::max(1u, lh / 2);
      std::vector<double> next(static_cast<size_t>(nw) * nh * ch);
      for (uint32_t y = 0; y < nh; ++y) {
        const uint32_t y0 = std::min(2 * y, lh - 1), y1 = std::min(2 * y + 1, lh - 1);
        for (uint32_t x = 0; x < nw; ++x) {
          const uint32_t x0 = std::min(2 * x, lw - 1), x1 = std::min(2 * x + 1, lw - 1);
          for (size_t c = 0; c < ch; ++c) {
            const auto at = [&](uint32_t xx, uint32_t yy) {
              return level[(static_cast<size_t>(yy) * lw + xx) * ch + c];
            };
            next[(static_cast<size_t>(y) * nw + x) * ch + c] =
                0.25 * (at(x0, y0) + at(x1, y0) + at(x0, y1) + at(x1, y1));
          }
        }
      }
      level = std::move(next);
      lw = nw;
      lh = nh;
    }
  }
  return texture;
}

wgpu::TextureView array_view(const wgpu::Texture& texture, int layers) {
  wgpu::TextureViewDescriptor vd{};
  vd.dimension = wgpu::TextureViewDimension::e2DArray;
  vd.arrayLayerCount = static_cast<uint32_t>(layers);
  return texture.CreateView(&vd);
}

}  // namespace

std::vector<uint8_t> moon_kind_map(int kind) {
  return decode(map_png(kind), kMoonMapsWidth, kMoonMapsHeight, 1);
}

std::vector<uint8_t> moon_kind_colour(int kind) {
  return decode(colour_png(kind), kMoonColourWidth, kMoonColourHeight, 3);
}

MoonMaps create_moon_maps(gpu::Device& device) {
  const int kinds = static_cast<int>(moon_kinds().size());
  MoonMaps m;
  // A map that does not decode is drawn at its mean, as the kind's tint alone.
  m.albedo = upload_array(device, "sky.moon_maps", kMoonMapsWidth, kMoonMapsHeight, kinds,
                          wgpu::TextureFormat::R8Unorm, 1, [](int k) {
                            std::vector<uint8_t> map = moon_kind_map(k);
                            if (map.empty()) {
                              map.assign(static_cast<size_t>(kMoonMapsWidth) * kMoonMapsHeight,
                                         static_cast<uint8_t>(kMoonMapMean));
                            }
                            return map;
                          });
  m.colour = upload_array(device, "sky.moon_colours", kMoonColourWidth, kMoonColourHeight, kinds,
                          wgpu::TextureFormat::RGBA8Unorm, 4, [](int k) {
                            const std::vector<uint8_t> rgb = moon_kind_colour(k);
                            const size_t n =
                                static_cast<size_t>(kMoonColourWidth) * kMoonColourHeight;
                            std::vector<uint8_t> rgba(n * 4, 128);
                            for (size_t i = 0; i < n && !rgb.empty(); ++i) {
                              for (size_t c = 0; c < 3; ++c) rgba[i * 4 + c] = rgb[i * 3 + c];
                            }
                            return rgba;
                          });
  m.albedo_view = array_view(m.albedo, kinds);
  m.colour_view = array_view(m.colour, kinds);
  return m;
}

}  // namespace ge::render
