// engine/texgen/look.hpp: each number reads what it says off a picture made to show it (a flat
// tile has no contrast, grain or edges; a checkerboard is all edges; red is warm and blue cool),
// and the distance is zero to itself, symmetric, and grows as two looks part.
//
// ge:summary Tests the look metrics and their distance on pictures made to show each number
// ge:keywords test texgen look brightness contrast grain edges warmth distance
// ge:skill testing textures
#include "texgen/look.hpp"

#include <cmath>

#include "check.hpp"

namespace tg = ge::texgen;
namespace lab = ge::lab;

namespace {

lab::Picture checker(int s, uint8_t a, uint8_t b) {
  lab::Picture p = lab::create_picture(s, s, {a, a, a});
  for (int y = 0; y < s; ++y) {
    for (int x = 0; x < s; ++x) {
      if (((x + y) & 1) == 0) continue;
      const size_t i = (static_cast<size_t>(y) * s + x) * 4;
      p.rgba[i] = p.rgba[i + 1] = p.rgba[i + 2] = b;
    }
  }
  return p;
}

}  // namespace

TEST(a_flat_tile_has_no_contrast_grain_or_edges) {
  const tg::Look l = tg::look_of(lab::create_picture(16, 16, {128, 128, 128}));
  CHECK(std::fabs(l.brightness - 128.0 / 255) < 1e-3);
  CHECK(l.contrast < 1e-6 && l.grain < 1e-9 && l.edges < 1e-9 && l.saturation < 1e-9);
}

TEST(a_checkerboard_is_all_edges_and_grain) {
  const tg::Look l = tg::look_of(checker(16, 40, 200));
  CHECK(l.edges > 0.99);
  CHECK(l.grain > 0.5);
  CHECK(l.contrast > 0.25);
}

TEST(red_is_warm_and_blue_is_cool) {
  const tg::Look red = tg::look_of(lab::create_picture(8, 8, {200, 60, 40}));
  const tg::Look blue = tg::look_of(lab::create_picture(8, 8, {40, 60, 200}));
  CHECK(red.warmth > 0.5 && blue.warmth < -0.5);
  CHECK(red.saturation > 0.5 && blue.saturation > 0.5);
}

TEST(the_distance_is_zero_to_itself_and_grows_apart) {
  const tg::Look a = tg::look_of(checker(16, 40, 200));
  const tg::Look b = tg::look_of(checker(16, 90, 150));
  const tg::Look c = tg::look_of(lab::create_picture(16, 16, {120, 120, 120}));
  CHECK(tg::look_distance(a, a) == 0);
  CHECK(tg::look_distance(a, b) == tg::look_distance(b, a));
  CHECK(tg::look_distance(a, b) < tg::look_distance(a, c));
  CHECK(tg::look_values(a)[0] == a.brightness && tg::look_values(a)[6] == a.colours);
}
