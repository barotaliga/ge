// tests/brush_map_fog_test.cpp: a classic map's own fog: the words of a `fog` key or command in
// every count from one to six, words that are not numbers refused, the worldspawn's keys (and the
// underscore's), its `skyfog`, and the renderer's flat fog made from it (metres, linear light, the
// sky share by the map's or the player's, times the alpha).
//
// ge:summary Tests for a classic map's fog and the renderer's flat fog made from it
// ge:keywords test brush fog map_fog parse_fog fog_words map_flat_fog skyfog worldspawn
// ge:skill brush testing
#include <cmath>
#include <string>

#include "brush/app/bsp_fog.hpp"
#include "brush/map.hpp"
#include "brush/map_fog.hpp"
#include "check.hpp"

namespace {

namespace gb = ge::brush;

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

gb::Map world_with(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  gb::Map m;
  gb::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  m.entities.push_back(e);
  return m;
}

TEST(the_words_read_in_every_count) {
  gb::MapFog f;
  CHECK(gb::parse_fog("0.1", &f));
  CHECK(near(f.density, 0.1) && near(f.colour[0], 0.3) && near(f.colour[2], 0.3));
  CHECK(gb::parse_fog("0.2 0.6", &f));
  CHECK(near(f.density, 0.2) && near(f.colour[0], 0.6) && near(f.colour[1], 0.6));
  CHECK(gb::parse_fog("1 0.5 0", &f));
  CHECK(near(f.density, 0.05) && near(f.colour[0], 1) && near(f.colour[1], 0.5));
  CHECK(gb::parse_fog("0.05 0.3 0.2 0.1", &f));
  CHECK(near(f.density, 0.05) && near(f.colour[2], 0.1) && near(f.alpha, 1) && near(f.bias, 0));
  CHECK(gb::parse_fog("0.05 0.3 0.2 0.1 0.7 0.4 9", &f));
  CHECK(near(f.alpha, 0.7) && near(f.bias, 0.4));
}

TEST(words_that_are_not_numbers_leave_the_fog_as_it_was) {
  gb::MapFog f;
  f.density = 0.3;
  CHECK(!gb::parse_fog("", &f));
  CHECK(!gb::parse_fog("thick grey", &f));
  CHECK(!gb::parse_fog("0.1 0.2x", &f));
  CHECK(near(f.density, 0.3));
}

TEST(the_command_keeps_the_maps_sky_share) {
  gb::MapFog f;
  f.sky_share = 0.2;
  CHECK(gb::parse_fog("0.05 1 1 1", &f));
  CHECK(near(f.sky_share, 0.2));
  gb::MapFog g;
  CHECK(gb::parse_fog(gb::fog_words(f), &g));
  CHECK(near(g.density, 0.05) && near(g.colour[1], 1) && near(g.alpha, 1));
}

TEST(the_worldspawn_keys) {
  CHECK(gb::map_fog(gb::Map{}).density == 0);
  CHECK(gb::map_fog(world_with({{"message", "a hall"}})).density == 0);
  const gb::MapFog a = gb::map_fog(world_with({{"fog", "0.05 0.3 0.2 0.1"}}));
  CHECK(near(a.density, 0.05) && a.sky_share < 0);
  const gb::MapFog b = gb::map_fog(world_with({{"_fog", "0.1 0.5 0.5 0.5"}, {"skyfog", "0.25"}}));
  CHECK(near(b.density, 0.1) && near(b.sky_share, 0.25));
  CHECK(gb::map_fog(world_with({{"fog", "a lot"}})).density == 0);
}

TEST(the_renderers_flat_fog) {
  gb::MapFog f;
  CHECK(ge::brush::app::map_flat_fog(f, 0.5).density == 0);  // no fog
  CHECK(gb::parse_fog("0.05 1 0.5 0 0.8", &f));
  const ge::render::FlatFog g = ge::brush::app::map_flat_fog(f, 0.5);
  // 0.05 / 64 a unit at 40 units a metre; half the light is fog about 1066 units out.
  CHECK(near(g.density, 0.05 / 64 / gb::kUnitM));
  const double t = g.density * 1066 * gb::kUnitM;
  CHECK(std::abs((1 - std::exp(-t * t)) - 0.5) < 0.01);
  CHECK(near(g.colour[0], 1) && near(g.colour[2], 0));
  CHECK(std::abs(g.colour[1] - 0.214) < 0.001);  // sRGB 0.5 in linear light
  CHECK(near(g.alpha, 0.8) && near(g.sky, 0.5 * 0.8));
  f.sky_share = 0.1;
  CHECK(near(ge::brush::app::map_flat_fog(f, 0.5).sky, 0.1 * 0.8));
}

}  // namespace
