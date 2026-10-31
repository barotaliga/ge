// plugins/brush/lights.hpp: a map's light entities read once (place, value, reach, colour by kind,
// style), entities that are not lights or have no origin skipped, and each as a bake's light at
// the real-time path's strength.
//
// ge:summary Test: a brush map's light entities and their bake lights
// ge:keywords test bsp brush light-entity bake-light light_entities bake_light_of
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
  const std::vector<LightEntity> ls = light_entities(m);
  CHECK(ls.size() == 3);
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
