// plugins/brush/lights.hpp: a map's light entities read once (place, value, reach, colour by kind,
// style), entities that are not lights or have no origin skipped, and each as a bake's light at
// the real-time path's strength; the fall-off kinds (`delay`), `_color`, the floors and surface
// templates skipped, and the spots aimed by `mangle` or a `target`.
//
// ge:summary Test: a brush map's light entities and their bake lights
// ge:keywords test bsp brush light-entity bake-light light_entities bake_light_of delay falloff
// ge:keywords spotlight mangle cone color-key
// ge:skill brush
#include "brush/lights.hpp"

#include <cmath>

#include "check.hpp"

using namespace ge::brush;

namespace {

Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  Entity e;
  e.pairs = std::move(pairs);
  return e;
}

}  // namespace

TEST(light_entities_reads_place_value_reach_colour_and_style) {
  Map m;
  m.entities.push_back(entity({{"classname", "worldspawn"}}));
  m.entities.push_back(
      entity({{"classname", "light"}, {"origin", "40 80 120"}, {"light", "200"}, {"wait", "2"}}));
  m.entities.push_back(
      entity({{"classname", "light_torch_small_walltorch"}, {"origin", "0 0 0"}, {"style", "6"}}));
  m.entities.push_back(entity({{"classname", "light_fluoro"}, {"origin", "1 2 3"}}));
  m.entities.push_back(entity({{"classname", "light"}}));  // no origin: skipped
  m.entities.push_back(entity({{"classname", "light_flame_large_yellow"}, {"origin", "0 0 0"}}));
  m.entities.push_back(entity({{"classname", "light_flame_small_white"}, {"origin", "0 0 0"}}));
  const std::vector<LightEntity> ls = light_entities(m);
  CHECK(ls.size() == 5);
  // A large flame (a brazier's) is told from a small one and from a torch.
  CHECK(ls[3].flame && ls[3].large && !ls[3].torch);
  CHECK(ls[4].flame && !ls[4].large && !ls[1].large);
  double want[3];
  const float at[3] = {40, 80, 120};
  to_engine(at, want);
  for (int k = 0; k < 3; ++k) CHECK(ls[0].pos[k] == want[k]);
  CHECK(ls[0].value == 200);
  CHECK(std::fabs(ls[0].reach - 200 / 2.0 * kUnitM) < 1e-12);
  CHECK(!ls[0].flame && !ls[0].torch);
  CHECK(ls[1].flame && ls[1].torch && ls[1].style == 6 && ls[1].value == 300);
  // A flame is warm, a tube cool; each colour has a luminance of one.
  CHECK(ls[1].colour[0] > ls[1].colour[2]);
  CHECK(ls[2].colour[2] > ls[2].colour[0]);
  for (const LightEntity& l : ls) {
    CHECK(std::fabs(0.2126 * l.colour[0] + 0.7152 * l.colour[1] + 0.0722 * l.colour[2] - 1) <
          1e-12);
  }
}

TEST(bake_light_of_matches_the_real_time_strength) {
  LightEntity l;
  l.value = 300;
  l.reach = 7.5;
  const ge::bake::Light b = bake_light_of(l, 2);
  const double half = 300 * kUnitM / 2;
  CHECK(std::fabs(b.colour[1] - 300 / 256.0 * half * half * 2) < 1e-9);
  CHECK(std::fabs(b.reach - 7.5 * 1.5) < 1e-12);
  CHECK(b.radius == 0.2);
  Map m;
  m.entities.push_back(entity({{"classname", "light"}, {"origin", "0 0 0"}}));
  CHECK(map_bake_lights(m).size() == 1);
}

TEST(a_falling_light_is_matched_at_its_distance_not_grown_as_the_cube_of_its_value) {
  Map m;
  // A bright inverse-square light and one of no fall-off, as a modern compiler's maps use them.
  m.entities.push_back(entity({{"classname", "light"},
                               {"origin", "0 0 0"},
                               {"light", "1000"},
                               {"wait", "0.5"},
                               {"delay", "5"}}));
  m.entities.push_back(
      entity({{"classname", "light"}, {"origin", "0 0 0"}, {"light", "2500"}, {"delay", "3"}}));
  m.entities.push_back(entity({{"classname", "light"}, {"origin", "0 0 0"}, {"light", "300"}}));
  const std::vector<LightEntity> ls = light_entities(m);
  CHECK(ls.size() == 3);
  // The strength a metre off, as the real-time path and the bake take it.
  const auto at1 = [](const LightEntity& l) {
    const double half = l.value * kUnitM / 2;
    return l.value / 256.0 * half * half * l.gain;
  };
  // Each falling light gives its value (of 128) at 128 / wait units, by an inverse square.
  for (int i = 0; i < 2; ++i) {
    const double units = i == 0 ? 256 : 128;
    const double d = units * kUnitM;
    CHECK(std::fabs(at1(ls[size_t(i)]) / (d * d) - ls[size_t(i)].value / 128) < 1e-9);
  }
  CHECK(ls[0].soft_m > 0 && ls[1].soft_m == 0);
  // A straight light is as it was; the 2500 is no longer hundreds of times the 300.
  CHECK(ls[2].gain == 1);
  CHECK(at1(ls[1]) < 40 * at1(ls[2]));
  CHECK(ls[0].reach < 64 && ls[1].reach < 64);
}

TEST(floors_and_surface_templates_are_not_lights) {
  Map m;
  m.entities.push_back(entity({{"classname", "light"}, {"origin", "0 0 0"}, {"delay", "4"}}));
  m.entities.push_back(
      entity({{"classname", "light"}, {"origin", "0 0 0"}, {"_surface", "window02_1"}}));
  m.entities.push_back(entity({{"classname", "light"}, {"origin", "0 0 0"}}));
  CHECK(light_entities(m).size() == 1);
}

TEST(a_colour_given_is_the_lights_own_at_a_luminance_of_one) {
  Map m;
  m.entities.push_back(
      entity({{"classname", "light"}, {"origin", "0 0 0"}, {"_color", "255 147 147"}}));
  m.entities.push_back(
      entity({{"classname", "light"}, {"origin", "0 0 0"}, {"_color", "0.2 0.4 1"}}));
  const std::vector<LightEntity> ls = light_entities(m);
  CHECK(ls.size() == 2);
  CHECK(ls[0].colour[0] > ls[0].colour[1] && std::fabs(ls[0].colour[1] - ls[0].colour[2]) < 1e-12);
  CHECK(ls[1].colour[2] > ls[1].colour[1] && ls[1].colour[1] > ls[1].colour[0]);
  for (const LightEntity& l : ls)
    CHECK(std::fabs(0.2126 * l.colour[0] + 0.7152 * l.colour[1] + 0.0722 * l.colour[2] - 1) <
          1e-12);
}

TEST(a_light_with_mangle_or_a_target_is_a_spot) {
  Map m;
  // Straight down by its pitch; along +x at a target; and a plain light.
  m.entities.push_back(entity(
      {{"classname", "light"}, {"origin", "0 0 100"}, {"mangle", "0 -90 0"}, {"angle", "60"}}));
  m.entities.push_back(entity({{"classname", "light"}, {"origin", "0 0 0"}, {"target", "spot1"}}));
  m.entities.push_back(
      entity({{"classname", "info_null"}, {"origin", "64 0 0"}, {"targetname", "spot1"}}));
  m.entities.push_back(entity({{"classname", "light"}, {"origin", "0 0 0"}, {"angle", "90"}}));
  const std::vector<LightEntity> ls = light_entities(m);
  CHECK(ls.size() == 3);
  double down[3], along[3];
  const float o[3] = {0, 0, 0}, z[3] = {0, 0, -1}, x[3] = {1, 0, 0};
  double zero[3];
  to_engine(o, zero);
  to_engine(z, down);
  to_engine(x, along);
  double dn = 0, al = 0;
  for (int k = 0; k < 3; ++k) {
    dn += (down[k] - zero[k]) * ls[0].dir[k];
    al += (along[k] - zero[k]) * ls[1].dir[k];
  }
  CHECK(ls[0].spot && dn > 0 && std::fabs(dn / kUnitM - 1) < 1e-6);
  CHECK(ls[1].spot && std::fabs(al / kUnitM - 1) < 1e-6);
  // The cone: its angle the whole cone, the full light inside three quarters of it.
  CHECK(std::fabs(ls[0].cone_cos[1] - std::cos(30 * 3.14159265358979323846 / 180)) < 1e-12);
  CHECK(ls[0].cone_cos[0] > ls[0].cone_cos[1]);
  CHECK(ls[1].cone_cos[1] < ls[1].cone_cos[0]);  // 40 when unset
  CHECK(!ls[2].spot);                            // a plain light's `angle` is not a cone
  // The bake takes the same spot.
  const ge::bake::Light b = bake_light_of(ls[0]);
  CHECK(b.kind == ge::bake::LightKind::spot && b.cone_cos[1] == ls[0].cone_cos[1]);
}
