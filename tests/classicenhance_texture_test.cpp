// One texture enhanced (plugins/classicenhance/texture.hpp), on pictures made here: every map at
// `scale` times the size; a flat colour stays that colour, flat in height and facing straight out;
// a hard vertical edge stays hard (no texel half way across it a texel away) and its normal map
// leans across it, not along it; the picture tiles (its first and last columns agree as a
// neighbour's would); fullbright texels and only they glow; a sky gets an albedo alone; and the
// result is the same bits every run.
//
// ge:summary Tests the texture enhancer: sizes, flat areas, hard edges, tiling, glow, determinism
// ge:keywords test classicenhance texture upscale normal height roughness emissive tile edge
// ge:skill classicenhance testing
#include <cstdlib>
#include <vector>

#include "check.hpp"
#include "classicenhance/texture.hpp"

namespace ce = ge::classicenhance;

namespace {

ce::Palette palette() {
  ce::Palette p;
  for (int i = 0; i < 256; i++) {
    p.rgb[size_t(i) * 3] = uint8_t(i);
    p.rgb[size_t(i) * 3 + 1] = uint8_t(i);
    p.rgb[size_t(i) * 3 + 2] = uint8_t(i);
  }
  p.rgb[size_t{230} * 3] = 255;  // a fullbright red
  p.rgb[230 * 3 + 1] = 40;
  p.rgb[230 * 3 + 2] = 20;
  return p;
}

int px(const ge::assets::Image& img, int x, int y, int c = 0) {
  return img.rgba[(size_t(y) * size_t(img.width) + size_t(x)) * 4 + size_t(c)];
}

}  // namespace

TEST(texture_flat_colour_stays_flat) {
  const std::vector<uint8_t> flat(size_t{16} * 16, 120);
  const auto e = ce::enhance_texture("wall_flat", flat, 16, 16, palette());
  CHECK(e.albedo.width == 64 && e.albedo.height == 64);
  CHECK(e.normal.width == 64 && e.height.width == 64 && e.roughness.width == 64);
  for (int y = 0; y < 64; y += 7) {
    for (int x = 0; x < 64; x += 5) {
      CHECK(std::abs(px(e.albedo, x, y) - 120) <= 1);
      CHECK(std::abs(px(e.normal, x, y, 0) - 128) <= 1);
      CHECK(std::abs(px(e.normal, x, y, 1) - 128) <= 1);
      CHECK(px(e.normal, x, y, 2) == 255);
    }
  }
  CHECK(!e.glows);
}

TEST(texture_hard_edge_stays_hard_and_tiles) {
  // Left half dark, right half light: an edge at x = 8 and, tiling, another at x = 0.
  std::vector<uint8_t> edge(size_t{16} * 16);
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) edge[size_t(y) * 16 + size_t(x)] = x < 8 ? 40 : 200;
  }
  ce::EnhanceOptions o;
  o.grain = 0;
  const auto e = ce::enhance_texture("metal_edge", edge, 16, 16, palette(), o);
  // Two output texels either side of the edge at x = 32 are within a few levels of each side.
  for (int y = 0; y < 64; y += 9) {
    CHECK(std::abs(px(e.albedo, 29, y) - 40) <= 12);
    CHECK(std::abs(px(e.albedo, 34, y) - 200) <= 12);
  }
  // The wrapped edge between x = 63 and 0 is drawn like the one in the middle, mirrored.
  for (int k = 0; k < 4; k++) {
    CHECK(std::abs(px(e.albedo, k, 10) - px(e.albedo, 31 - k, 10)) <= 3);
    CHECK(std::abs(px(e.albedo, 63 - k, 10) - px(e.albedo, 32 + k, 10)) <= 3);
  }
  // The normal map leans across the edge (red moves) and not along it (green stays).
  int lean = 0;
  for (int x = 26; x < 38; x++) lean = std::max(lean, std::abs(px(e.normal, x, 20, 0) - 128));
  CHECK(lean > 20);
  CHECK(std::abs(px(e.normal, 32, 20, 1) - 128) <= 2);
  CHECK(e.material == ce::Material::metal);
}

TEST(texture_glow_only_where_fullbright) {
  std::vector<uint8_t> lamp(size_t{16} * 16, 60);
  for (int y = 6; y < 10; y++) {
    for (int x = 6; x < 10; x++) lamp[size_t(y) * 16 + size_t(x)] = 230;
  }
  const auto e = ce::enhance_texture("tlight_lamp", lamp, 16, 16, palette());
  CHECK(e.glows);
  CHECK(px(e.emissive, 32, 32, 0) > 200);  // the lamp's middle glows red
  CHECK(px(e.emissive, 4, 4, 0) == 0);     // the wall round it does not
}

TEST(texture_sky_and_liquid) {
  const std::vector<uint8_t> pic(size_t{32} * 16, 90);
  const auto sky = ce::enhance_texture("sky4", pic, 32, 16, palette());
  CHECK(sky.material == ce::Material::sky);
  CHECK(sky.albedo.width == 128 && sky.normal.width == 0);
  const auto water = ce::enhance_texture("*water0", pic, 32, 16, palette());
  CHECK(water.material == ce::Material::liquid);
  CHECK(px(water.roughness, 3, 3) < 40);
  CHECK(ce::never_drawn("TRIGGER") && ce::never_drawn("clip") && !ce::never_drawn("tech01_1"));
  CHECK(ce::material_of("wbrick1_5") == ce::Material::stone);
  CHECK(ce::material_of("crate0_side") == ce::Material::wood);
}

TEST(texture_same_bits_every_run) {
  std::vector<uint8_t> noise(size_t{32} * 32);
  uint32_t s = 7;
  for (uint8_t& v : noise) {
    s = s * 1664525u + 1013904223u;
    v = uint8_t(s >> 24);
  }
  const auto a = ce::enhance_texture("rock_noise", noise, 32, 32, palette());
  const auto b = ce::enhance_texture("rock_noise", noise, 32, 32, palette());
  CHECK(a.albedo.rgba == b.albedo.rgba && a.normal.rgba == b.normal.rgba);
  CHECK(a.height.rgba == b.height.rgba && a.roughness.rgba == b.roughness.rgba);
  ce::EnhanceOptions two;
  two.scale = 2;
  CHECK(ce::enhance_texture("rock_noise", noise, 32, 32, palette(), two).albedo.width == 64);
}
