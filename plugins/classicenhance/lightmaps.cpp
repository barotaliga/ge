// The lightmap stage (lightmaps.hpp): the shelf packer and the stage's one write.
//
// ge:summary The lightmap stage's shelf packer and its write into the cache
// ge:keywords lightmap atlas shelf pack write png
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include "classicenhance/lightmaps.hpp"

#include <algorithm>
#include <vector>

#include "classicenhance/cache.hpp"
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

bool light_map(const brush::Map& map, const std::string& name, const std::filesystem::path& dir,
               std::string* error) {
  const assets::Image img = pack_lightmaps(map);
  const std::vector<uint8_t> png = lab::encode_png({img.width, img.height, img.rgba});
  if (!write_whole(dir / (safe_name(name) + ".png"), png)) {
    *error = "cannot write " + (dir / (safe_name(name) + ".png")).string() +
             "; check the cache folder is writable (GE_CLASSIC_CACHE)";
    return false;
  }
  return true;
}

}  // namespace ge::classicenhance
