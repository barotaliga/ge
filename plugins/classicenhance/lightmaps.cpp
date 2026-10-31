// The lightmap stage (lightmaps.hpp): the textures' means, the rebake and its write; the shelf
// packer of the map's own light.
//
// ge:summary The lightmap stage's texture means, rebake and write; the own light's shelf packer
// ge:keywords lightmap atlas shelf pack write gerb rebake texture-means
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include "classicenhance/lightmaps.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include "brush/lights.hpp"
#include "classicenhance/cache.hpp"
#include "core/libm/libm.hpp"
#include "lab/png.hpp"

namespace ge::classicenhance {

assets::Image pack_lightmaps(const brush::Map& map, int max_width) {
  struct Place {
    int x, y;
  };
  std::vector<Place> at(map.lightmaps.size());
  int x = 0, y = 0, row = 0, width = 1;
  for (size_t i = 0; i < map.lightmaps.size(); i++) {
    const int w = int(map.lightmaps[i].width) + 2, h = int(map.lightmaps[i].height) + 2;
    if (x + w > max_width && x > 0) {
      x = 0;
      y += row;
      row = 0;
    }
    at[i] = {x + 1, y + 1};
    x += w;
    row = std::max(row, h);
    width = std::max(width, x);
  }
  const int height = std::max(1, y + row);
  assets::Image img{width, height, std::vector<uint8_t>(size_t(width) * size_t(height) * 4, 0)};
  for (size_t i = 0; i < map.lightmaps.size(); i++) {
    const brush::Lightmap& lm = map.lightmaps[i];
    for (uint32_t ly = 0; ly < lm.height; ly++) {
      for (uint32_t lx = 0; lx < lm.width; lx++) {
        const size_t src = lm.offset + (size_t(ly) * lm.width + lx) * 3;
        if (src + 2 >= map.light_rgb.size()) continue;
        const size_t dst =
            (size_t(at[i].y + int(ly)) * size_t(width) + size_t(at[i].x + int(lx))) * 4;
        img.rgba[dst] = map.light_rgb[src];
        img.rgba[dst + 1] = map.light_rgb[src + 1];
        img.rgba[dst + 2] = map.light_rgb[src + 2];
        img.rgba[dst + 3] = 255;
      }
    }
  }
  return img;
}

namespace {

double linear_of(uint8_t v) {
  const double c = v / 255.0;
  return c <= 0.04045 ? c / 12.92 : core::libm::pow((c + 0.055) / 1.055, 2.4);
}

}  // namespace

std::vector<std::array<float, 3>> texture_means(const brush::Map& map,
                                                const std::filesystem::path& cache,
                                                std::span<const uint8_t> palette) {
  std::vector<std::array<float, 3>> out(map.textures.size(), {0.5f, 0.5f, 0.5f});
  for (size_t t = 0; t < map.textures.size(); t++) {
    const brush::Texture& tex = map.textures[t];
    double sum[3] = {0, 0, 0}, n = 0;
    std::optional<lab::Picture> pic;
    if (!cache.empty() && !tex.name.empty()) {
      pic = lab::read_png((cache / "textures" / safe_name(tex.name) / "albedo.png").string());
    }
    if (pic && !pic->rgba.empty()) {
      const std::vector<uint8_t>& px = pic->rgba;
      for (size_t i = 0; i + 3 < px.size(); i += 4) {
        for (int k = 0; k < 3; k++) sum[k] += linear_of(px[i + size_t(k)]);
        n += 1;
      }
    } else if (palette.size() >= 768) {
      for (const uint8_t ix : tex.indexed) {
        for (int k = 0; k < 3; k++) sum[k] += linear_of(palette[size_t(ix) * 3 + size_t(k)]);
        n += 1;
      }
    }
    if (n > 0) {
      for (int k = 0; k < 3; k++) out[t][size_t(k)] = float(sum[k] / n);
    }
  }
  return out;
}

bool light_map(const brush::Map& map, const std::string& name, const std::filesystem::path& dir,
               std::span<const std::array<float, 3>> albedo, std::string* error,
               const LightmapOptions& options, brush::Rebaked* out, size_t* bytes) {
  brush::RebakeInput in;
  in.albedo.assign(albedo.begin(), albedo.end());
  in.lights = brush::map_bake_lights(map);
  brush::RebakeOptions ro;
  ro.luxel_m = options.luxel_m;
  ro.bake.samples = options.samples;
  ro.bake.bounces = options.bounces;
  brush::Rebaked baked;
  if (!brush::rebake_map(map, in, ro, &baked, error)) {
    *error = name + ": " + *error;
    return false;
  }
  const std::vector<uint8_t> file = brush::write_rebaked(baked);
  const std::filesystem::path at = dir / (safe_name(name) + ".gerb");
  if (!write_whole(at, file)) {
    *error =
        "cannot write " + at.string() + "; check the cache folder is writable (GE_CLASSIC_CACHE)";
    return false;
  }
  if (bytes) *bytes = file.size();
  if (out) *out = std::move(baked);
  return true;
}

}  // namespace ge::classicenhance
