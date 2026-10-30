// The BSP renderer's scene (plugins/brush/app/draw_data.hpp) with a texture source: a texture
// handed at twice the map's size gets a rectangle twice as large in an atlas of six mips, and its
// faces' texture coordinates are doubled so the picture covers them as the map's own did; one at
// a size that is no multiple keeps the map's own; and with no source the atlas is as before.
//
// ge:summary Tests the BSP scene's texture source: upscaled textures, their rects, st and mips
// ge:keywords test brush scene load_scene texture-source upscaled atlas mips st rect
// ge:skill brush testing
#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace ba = ge::brush::app;

namespace {

ba::Pack fixture_pack() {
  std::vector<uint8_t> map;
  std::string error;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &map, &error),
            error);
  ba::Pack pack;
  pack.bytes = map;
  pack.files.push_back({"maps/hall.bsp", 0, uint32_t(map.size())});
  return pack;
}

std::vector<uint8_t> grey_palette() {
  std::vector<uint8_t> p(768);
  for (size_t i = 0; i < p.size(); i++) p[i] = uint8_t(i / 3);
  return p;
}

}  // namespace

TEST(an_upscaled_texture_gets_a_larger_rect_and_scaled_coordinates) {
  const ba::Pack pack = fixture_pack();
  const std::vector<uint8_t> palette = grey_palette();
  ba::Scene own;
  std::string error;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &own, &error), error);
  CHECK(own.textures.mips == 4 && own.textures.levels.size() == 4);
  CHECK(!own.map.textures.empty() && !own.vertices.empty());

  // Every texture but the first handed back at twice its size.
  struct Twice {
    std::string skip;
    static bool replace(void* user, std::string_view name, uint32_t w, uint32_t h,
                        std::span<const uint8_t>, std::span<const uint8_t>,
                        std::vector<uint8_t>* rgba) {
      if (name == static_cast<Twice*>(user)->skip) return false;
      rgba->assign(size_t(w) * 2 * h * 2 * 4, 77);
      return true;
    }
  } twice{own.map.textures[0].name};
  ba::Scene big;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &big, &error, {&Twice::replace, &twice}),
            error);
  CHECK(big.textures.mips == 6 && big.textures.levels.size() == 6);
  CHECK(big.vertices.size() == own.vertices.size());
  int doubled = 0;
  for (size_t i = 0; i < own.vertices.size(); i++) {
    const ba::DrawVertex& a = own.vertices[i];
    const ba::DrawVertex& b = big.vertices[i];
    if (b.rect[2] != 2 * a.rect[2]) {
      CHECK(b.rect[2] == a.rect[2] && b.st[0] == a.st[0]);
      continue;
    }
    CHECK(b.rect[3] == 2 * a.rect[3] && b.st[0] == 2 * a.st[0] && b.st[1] == 2 * a.st[1]);
    // The replaced texels where its rect says.
    const size_t at = (size_t(b.rect[1]) * big.textures.width + size_t(b.rect[0])) * 4;
    CHECK(big.textures.levels[0][at] == 77);
    doubled++;
  }
  CHECK(doubled > 0);
}
