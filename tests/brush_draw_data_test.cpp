// The BSP renderer's scene (plugins/brush/app/draw_data.hpp) with a texture source: a texture
// handed at twice the map's size gets a rectangle twice as large in an atlas of six mips, and its
// faces' texture coordinates are doubled so the picture covers them as the map's own did; one at
// a size that is no multiple keeps the map's own; and with no source the atlas is as before.
// A replaced texture's relief lands in the surface atlas at its rectangle and mips, flat elsewhere.
// A light is seen by its fire, its globe or a glowing face within kFixtureM; a fill light is not.
// A `.lit` file colours the map's light (and the lightmap atlas); one from another build is
// refused.
//
// ge:summary Tests the BSP scene's texture source: upscaled textures, their rects, st and mips
// ge:keywords test brush scene load_scene texture-source upscaled atlas mips st rect lit
// ge:keywords surface-atlas relief seen-lights fill-light fixture
// ge:skill brush testing
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/pictures.hpp"
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

TEST(a_replaced_textures_relief_lands_in_the_surface_atlas_at_its_rect) {
  const ba::Pack pack = fixture_pack();
  const std::vector<uint8_t> palette = grey_palette();
  ba::Scene own;
  std::string error;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &own, &error), error);
  CHECK(own.surface.levels.empty());  // no source: no relief

  // Every texture twice its size; every one's relief but the first's: a slope to the right.
  struct Relief {
    std::string flat;
    static bool replace(void*, std::string_view, uint32_t w, uint32_t h, std::span<const uint8_t>,
                        std::span<const uint8_t>, std::vector<uint8_t>* rgba) {
      rgba->assign(size_t(w) * 2 * h * 2 * 4, 77);
      return true;
    }
    static bool surface(void* user, std::string_view name, uint32_t w, uint32_t h,
                        std::vector<uint8_t>* rgba) {
      if (name == static_cast<Relief*>(user)->flat) return false;
      rgba->resize(size_t(w) * h * 4);
      for (size_t i = 0; i < rgba->size(); i += 4) {
        (*rgba)[i] = 200;
        (*rgba)[i + 1] = 128;
        (*rgba)[i + 2] = 64;
        (*rgba)[i + 3] = 30;
      }
      return true;
    }
  } relief{own.map.textures[0].name};
  ba::TextureSource source{&Relief::replace, &relief, &Relief::surface};
  ba::Scene big;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &big, &error, source), error);
  CHECK(big.surface.width == big.textures.width && big.surface.height == big.textures.height);
  CHECK(big.surface.mips == big.textures.mips && big.surface.levels.size() == big.textures.mips);
  int with = 0, flat = 0;
  for (const ba::DrawVertex& v : big.vertices) {
    for (uint32_t k : {0u, 2u}) {
      const uint32_t w = big.surface.width >> k;
      const size_t at = (size_t(uint32_t(v.rect[1]) >> k) * w + (uint32_t(v.rect[0]) >> k)) * 4;
      const uint8_t* t = &big.surface.levels[k][at];
      const bool sloped = t[0] == 200 && t[1] == 128 && t[2] == 64 && t[3] == 30;
      const bool level =
          t[0] == ba::kFlatSurface[0] && t[2] == ba::kFlatSurface[2] && t[3] == ba::kFlatSurface[3];
      CHECK(sloped || level);
      (sloped ? with : flat)++;
    }
  }
  CHECK(with > 0 && flat > 0);
}

TEST(a_full_colour_pictures_alpha_and_a_fence_textures_holes_make_the_cover) {
  const ba::Pack pack = fixture_pack();
  const std::vector<uint8_t> palette = grey_palette();
  ba::Scene own;
  std::string error;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &own, &error), error);
  CHECK(own.cover.levels.empty());  // every texel solid: no cover, the shader's stand-in

  // Texture 0 handed as a 100 x 60 picture, its left half see-through: the map's 64 x 64 times 2
  // holds it, so its rect doubles and its cover is 0 on the left, 255 on the right.
  std::vector<ge::brush::MapPicture> pics = ge::brush::read_map_pictures(pack.bytes);
  CHECK(!pics.empty());
  ge::brush::MapPicture& p = pics[0];
  p.rgba_width = 100;
  p.rgba_height = 60;
  p.rgba.resize(size_t{100} * 60 * 4);
  for (uint32_t y = 0; y < 60; ++y)
    for (uint32_t x = 0; x < 100; ++x) {
      uint8_t* t = &p.rgba[(size_t(y) * 100 + x) * 4];
      t[0] = 200, t[1] = 100, t[2] = 50, t[3] = x < 50 ? 0 : 255;
    }
  ba::TextureSource source;
  source.pictures = &pics;
  ba::Scene cut;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &cut, &error, source), error);
  CHECK(cut.cover.levels.size() == cut.textures.levels.size());
  CHECK(cut.cover.levels[0].size() == size_t(cut.cover.width) * cut.cover.height);
  bool seen = false;
  for (size_t i = 0; i < cut.vertices.size() && !seen; ++i) {
    const ba::DrawVertex& v = cut.vertices[i];
    if (v.rect[2] != 128 || v.rect[3] != 128) continue;  // texture 0, doubled
    seen = true;
    const size_t row = size_t(v.rect[1] + 64) * cut.cover.width;
    CHECK(cut.cover.levels[0][row + size_t(v.rect[0]) + 10] == 0);
    CHECK(cut.cover.levels[0][row + size_t(v.rect[0]) + 120] == 255);
    const size_t at = (row + size_t(v.rect[0]) + 120) * 4;
    CHECK(cut.textures.levels[0][at] == 200 && cut.textures.levels[0][at + 3] == 0);  // no glow
  }
  CHECK(seen);

  // A fence texture (a name starting `{`): its index 255 is a hole.
  ge::brush::Fixture fx = ge::brush::sample_fixture();
  for (ge::brush::FixtureRoom& r : fx.rooms) r.floor = "{grate";
  std::vector<uint8_t> map;
  CHECK_MSG(ge::brush::write_bsp(fx, ge::brush::Family::v29, &map, &error), error);
  ba::Pack fence;
  fence.bytes = map;
  fence.files.push_back({"maps/fence.bsp", 0, uint32_t(map.size())});
  std::vector<ge::brush::MapPicture> fp = ge::brush::read_map_pictures(map);
  std::vector<uint8_t> holes(size_t{64} * 64, 3);
  for (size_t t = 0; t < 64; ++t) holes[t] = 255;  // its first row
  size_t grate = fp.size();
  for (size_t i = 0; i < fp.size(); ++i)
    if (fp[i].name == "{grate") grate = i, fp[i].level[0] = holes.data();
  CHECK(grate < fp.size());
  ba::TextureSource fs;
  fs.pictures = &fp;
  ba::Scene fenced;
  CHECK_MSG(ba::load_scene(fence, "maps/fence.bsp", palette, &fenced, &error, fs), error);
  CHECK(!fenced.cover.levels.empty());
  size_t zeros = 0;
  for (uint8_t c : fenced.cover.levels[0]) zeros += c == 0;
  CHECK(zeros == 64);
}

TEST(a_lit_file_colours_the_light_and_one_from_another_build_is_refused) {
  const ba::Pack pack = fixture_pack();
  const std::vector<uint8_t> palette = grey_palette();
  ba::Scene own;
  std::string error;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &own, &error), error);
  const size_t luxels = own.map.light_rgb.size();
  CHECK(luxels > 0 && luxels % 3 == 0);
  // A version 1 QLIT file: every luxel pure red.
  std::vector<uint8_t> lit = {'Q', 'L', 'I', 'T', 1, 0, 0, 0};
  for (size_t i = 0; i < luxels; i += 3) lit.insert(lit.end(), {200, 0, 0});
  std::string refused;
  ba::Scene red;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &red, &error, {}, lit, &refused), error);
  CHECK(refused.empty());
  CHECK(red.map.light_rgb[0] == 200 && red.map.light_rgb[1] == 0 && red.map.light_rgb[2] == 0);
  // The lightmap atlas carries it: some luxel red with no green, where the grey map had none.
  const auto reddish = [](const ba::Scene& s) {
    const std::vector<uint8_t>& l = s.light.levels[0];
    for (size_t i = 0; i + 3 < l.size(); i += 4)
      if (l[i] > 0 && l[i + 1] == 0 && l[i + 2] == 0) return true;
    return false;
  };
  CHECK(reddish(red) && !reddish(own));
  // One luxel short: another compile's light, refused, and the map keeps its own.
  lit.resize(lit.size() - 3);
  ba::Scene kept;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &kept, &error, {}, lit, &refused),
            error);
  CHECK(!refused.empty());
  CHECK(kept.map.light_rgb == own.map.light_rgb);
}

TEST(a_light_is_seen_by_its_fire_its_globe_or_a_glowing_face_within_reach) {
  const ba::Pack pack = fixture_pack();
  const std::vector<uint8_t> palette = grey_palette();
  ba::Scene s;
  std::string error;
  CHECK_MSG(ba::load_scene(pack, "maps/hall.bsp", palette, &s, &error), error);
  CHECK(!s.face_runs.empty() && !s.textures.levels.empty());
  // A wall's point: the first drawn solid face's first vertex.
  const float* wall = nullptr;
  for (const auto& [first, count] : s.face_runs) {
    if (count >= 3 && s.vertices[s.indices[first]].kind == uint32_t(ba::kSolid)) {
      wall = s.vertices[s.indices[first]].pos;
      break;
    }
  }
  CHECK(wall != nullptr);
  if (wall == nullptr) return;
  // A point in the air: the one of a half-metre grid furthest from every drawn face's box.
  const auto off_faces = [&](const double q[3]) {
    double nearest = 1e30;
    for (const auto& [first, count] : s.face_runs) {
      if (count < 3) continue;
      double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
      for (uint32_t i = first; i < first + count; ++i)
        for (int k = 0; k < 3; ++k) {
          lo[k] = std::min(lo[k], double(s.vertices[s.indices[i]].pos[k]));
          hi[k] = std::max(hi[k], double(s.vertices[s.indices[i]].pos[k]));
        }
      double d2 = 0;
      for (int k = 0; k < 3; ++k) {
        const double out = std::max({lo[k] - q[k], q[k] - hi[k], 0.0});
        d2 += out * out;
      }
      nearest = std::min(nearest, d2);
    }
    return std::sqrt(nearest);
  };
  double mid[3] = {0, 0, 0}, best = -1;
  const auto steps = [&](int k) { return int((s.maxs[k] - s.mins[k]) / 0.5); };
  for (int i = 0; i <= steps(0); ++i)
    for (int j = 0; j <= steps(1); ++j)
      for (int m = 0; m <= steps(2); ++m) {
        const double q[3] = {s.mins[0] + 0.5 * i, s.mins[1] + 0.5 * j, s.mins[2] + 0.5 * m};
        if (const double d = off_faces(q); d > best) {
          best = d;
          for (int k = 0; k < 3; ++k) mid[k] = q[k];
        }
      }
  CHECK_MSG(best > 2 * ba::kFixtureM,
            "the fixture's hall should hold a point a few metres off every face");
  const auto light_at = [](const double p[3]) {
    ba::PointLight l;
    for (int k = 0; k < 3; ++k) l.pos[k] = p[k];
    return l;
  };
  const double near[3] = {wall[0] + 0.5 * ba::kFixtureM, double(wall[1]), double(wall[2])};
  s.lights = {light_at(near), light_at(mid), light_at(mid), light_at(mid)};
  s.lights[2].flame = true;
  s.lights[3].globe = true;
  // No texel glows: only the flame and the globe show.
  for (size_t i = 3; i < s.textures.levels[0].size(); i += 4) s.textures.levels[0][i] = 0;
  CHECK(ba::seen_lights(s) == 2);
  CHECK(!s.lights[0].seen && !s.lights[1].seen && s.lights[2].seen && s.lights[3].seen);
  // Every texel glows: the light beside a wall is at its fitting, the one in the air is not.
  for (size_t i = 3; i < s.textures.levels[0].size(); i += 4) s.textures.levels[0][i] = 255;
  CHECK(ba::seen_lights(s) == 3);
  CHECK(s.lights[0].seen && !s.lights[1].seen);
}
