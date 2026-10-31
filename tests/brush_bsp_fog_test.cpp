// plugins/brush/app/bsp_fog.hpp: a classic map's own areas of fog. A `fog_volume` point entity
// is read into a volume in engine metres with its keys (the wind's axes turned to the engine's),
// a brush one into its model's box, and anything else, or one with no box, into none; the fog
// fixture's hall dust takes no light from the sky (its windows are in a wall) while a volume over
// its yard takes all of it; a pool of lava gets a glowing haze over its top and a pool of slime a
// faint green one, each switched off by its row.
//
// ge:summary Tests for a classic map's own fog: entity keys, brush boxes, sky share, liquids' hazes
// ge:keywords test brush bsp fog fog-volume fog_volume sky-share lava-haze slime-haze
// ge:skill brush testing
#include <cmath>
#include <string>
#include <vector>

#include "brush/app/bsp_fog.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;
namespace br = ge::brush;
namespace gr = ge::render;

bool near(double a, double b, double tol = 1e-4) { return std::fabs(a - b) <= tol; }

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// The fog fixture loaded as a scene (a grey palette).
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

ba::LiquidPatch pool(ba::Kind kind, double level, double x0, double z0, double x1, double z1) {
  ba::LiquidPatch p;
  p.kind = kind;
  p.level = level;
  p.box[0] = x0, p.box[1] = z0, p.box[2] = x1, p.box[3] = z1;
  p.area = (x1 - x0) * (z1 - z0);
  return p;
}

}  // namespace

TEST(a_point_fog_volume_is_read_with_its_keys) {
  const br::Map map;
  gr::FogVolume v;
  CHECK(ba::fog_volume_of(map,
                          entity({{"classname", "fog_volume"},
                                  {"origin", "40 -80 120"},
                                  {"size", "160 80 240"},
                                  {"density", "0.12"},
                                  {"albedo", "0.5 0.6 0.7"},
                                  {"glow", "1 2 3"},
                                  {"falloff", "0.3"},
                                  {"phase", "0.5"},
                                  {"wind", "1 2 3"},
                                  {"sky", "0.25"}}),
                          &v));
  // Map units at 0.025 m, z up: x stays, the map's y is the engine's z, its z the engine's y.
  CHECK(near(v.centre[0], 1) && near(v.centre[1], 3) && near(v.centre[2], -2));
  CHECK(near(v.half[0], 2) && near(v.half[1], 3) && near(v.half[2], 1));
  CHECK(near(v.density, 0.12) && near(v.falloff, 0.3) && near(v.phase, 0.5));
  CHECK(near(v.albedo[0], 0.5) && near(v.albedo[2], 0.7) && near(v.glow[1], 2));
  CHECK(near(v.wind[0], 1) && near(v.wind[1], 3) && near(v.wind[2], 2));
  CHECK(near(v.sky, 0.25));
  // Without a sky key it is left to be measured.
  CHECK(ba::fog_volume_of(
      map, entity({{"classname", "fog_volume"}, {"origin", "0 0 0"}, {"size", "8 8 8"}}), &v));
  CHECK(v.sky < 0);
}

TEST(a_brush_fog_volume_takes_its_models_box) {
  br::Map map;
  map.models.resize(2);
  map.models[1].mins[0] = 0, map.models[1].mins[1] = 0, map.models[1].mins[2] = 0;
  map.models[1].maxs[0] = 400, map.models[1].maxs[1] = 200, map.models[1].maxs[2] = 80;
  gr::FogVolume v;
  CHECK(ba::fog_volume_of(map, entity({{"classname", "fog_volume"}, {"model", "*1"}}), &v));
  CHECK(near(v.centre[0], 5) && near(v.centre[1], 1) && near(v.centre[2], 2.5));
  CHECK(near(v.half[0], 5) && near(v.half[1], 1) && near(v.half[2], 2.5));
  // The world's own model, a model the map lacks, another class and no box: none.
  CHECK(!ba::fog_volume_of(map, entity({{"classname", "fog_volume"}, {"model", "*0"}}), &v));
  CHECK(!ba::fog_volume_of(map, entity({{"classname", "fog_volume"}, {"model", "*7"}}), &v));
  CHECK(!ba::fog_volume_of(
      map, entity({{"classname", "light"}, {"origin", "0 0 0"}, {"size", "8 8 8"}}), &v));
  CHECK(!ba::fog_volume_of(map, entity({{"classname", "fog_volume"}, {"origin", "0 0 0"}}), &v));
}

TEST(the_halls_dust_takes_no_sky_and_the_yards_air_all_of_it) {
  const ba::Scene s = fixture_scene();
  ba::FogRows rows;
  const std::vector<gr::FogVolume> fog = ba::bsp_fog(s, rows);
  CHECK(fog.size() == 1);
  CHECK(near(fog[0].density, 0.07) && near(fog[0].phase, 0.6));
  CHECK(fog[0].sky == 0.0f);  // its windows are in its wall: straight up is its ceiling
  gr::FogVolume yard;
  yard.centre[0] = 8, yard.centre[1] = 2.5, yard.centre[2] = 20;
  yard.half[0] = 7, yard.half[1] = 2.4, yard.half[2] = 18;
  CHECK(near(ba::sky_share(s, yard), 1));
  rows.placed = false;
  CHECK(ba::bsp_fog(s, rows).empty());
}

TEST(lava_glows_through_its_haze_and_slime_faintly) {
  ba::Scene s = fixture_scene();
  s.liquids.push_back(pool(ba::kLava, -0.25, 5.6, -15.2, 10.4, -11.6));  // the store's pit
  s.liquids.push_back(pool(ba::kSlime, 0.0, 2, 4, 6, 8));                // out in the yard
  ba::FogRows rows;
  rows.placed = false;
  const std::vector<gr::FogVolume> fog = ba::bsp_fog(s, rows);
  CHECK(fog.size() == 2);
  const gr::FogVolume& lava = fog[0];
  CHECK(near(lava.centre[0], 8) && near(lava.centre[2], -13.4));
  CHECK(near(lava.centre[1] - lava.half[1], -0.25));  // its floor on the melt
  CHECK(near(2 * lava.half[1], rows.lava_height));
  CHECK(lava.glow[0] > lava.glow[1] && lava.glow[1] > lava.glow[2] && lava.glow[2] > 0);
  CHECK(lava.flicker > 0 && lava.falloff > 0 && lava.wind[1] > 0);  // thinning as it rises
  CHECK(lava.sky == 0.0f);                                          // the store is closed
  const gr::FogVolume& slime = fog[1];
  CHECK(slime.glow[1] > slime.glow[0] && slime.glow[1] < lava.glow[0]);
  CHECK(slime.sky == 1.0f);
  rows.lava = false;
  CHECK(ba::bsp_fog(s, rows).size() == 1);
  rows.slime = false;
  CHECK(ba::bsp_fog(s, rows).empty());
}
