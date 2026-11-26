// plugins/brush/app/bsp_liquids.hpp's `eye_water_body`: the water body an eye is under, which the
// water's view from below (Snell's window onto the world over the surface) is drawn for. An eye in
// a water leaf under a pool's level is in that pool; an eye in a lava or a slime leaf is under no
// water, even with a pool's level over it elsewhere on the map (the window was drawn in the lava
// and the slime); an eye in the air is in none.
//
// ge:summary Tests for the water body an eye is under: water's leaves only, never lava or slime
// ge:keywords test brush bsp liquids eye_water_body under-water snell-window lava slime eye-in
// ge:skill brush testing
#include <cstdint>
#include <string>
#include <vector>

#include "brush/app/bsp_liquids.hpp"
#include "brush/map.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;
namespace br = ge::brush;

// The fog fixture loaded as a scene (a grey palette): its store room's pit is the place the eye is.
ba::Scene fixture_scene() {
  std::vector<uint8_t> map;
  std::string error;
  CHECK_MSG(br::write_bsp(br::fog_fixture(), br::Family::v29, &map, &error), error);
  ba::Pack pack;
  pack.bytes = map;
  pack.files.push_back({"maps/fog.bsp", 0, uint32_t(map.size())});
  std::vector<uint8_t> palette(768);
  for (size_t i = 0; i < palette.size(); i++) palette[i] = uint8_t(i / 3);
  ba::Scene s;
  CHECK_MSG(ba::load_scene(pack, "maps/fog.bsp", palette, &s, &error), error);
  return s;
}

// The eye in the pit (map units 320 -536 -16; engine metres x, up, z = x, map z, map y).
constexpr double kEye[3] = {8.0, -0.4, -13.4};

// One water body whose level (0 m) stands over the eye, its box round the eye's plan.
ba::LiquidWorld one_pool_over_the_eye() {
  ba::LiquidWorld w;
  ge::water::Body b;
  b.level = 0;
  b.box[0] = 5, b.box[1] = -16, b.box[2] = 11, b.box[3] = -11;
  w.world.bodies.push_back(b);
  w.kinds.push_back(ba::kWater);
  return w;
}

// The eye's leaf given `contents` (the version 29 numbers: -1 empty, -3 water, -4 slime, -5 lava).
int body_with_contents(int contents) {
  ba::Scene s = fixture_scene();
  const float p[3] = {320, -536, -16};
  s.map.leaves[br::leaf_at(s.map, p)].contents = contents;
  return ba::eye_water_body(s.map, one_pool_over_the_eye(), kEye);
}

TEST(an_eye_in_water_under_a_pools_level_is_in_that_pool) { CHECK(body_with_contents(-3) == 0); }

TEST(an_eye_in_lava_or_slime_is_under_no_water) {
  CHECK(body_with_contents(-5) == -1);
  CHECK(body_with_contents(-4) == -1);
}

TEST(an_eye_in_the_air_is_in_no_body) { CHECK(body_with_contents(-1) == -1); }

}  // namespace
