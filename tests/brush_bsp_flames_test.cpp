// plugins/brush/app/bsp_flames.hpp: a wall torch's and a large flame's light entities become
// flames over their origins, a plain lamp none, and each fire's level follows its light's flicker.
//
// ge:summary Tests for the map's fires: which lights burn, where, and their level
// ge:keywords test brush bsp torch flame fire bsp_fires flicker-sync
// ge:skill brush testing
#include <cmath>

#include "brush/app/bsp_flames.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;

ba::Scene three_lights() {
  ba::Scene s;
  ba::PointLight torch, large, lamp;
  torch.pos[0] = 1;
  torch.flame = true;
  torch.fire = ba::Fire::wall_torch;
  torch.flicker = ba::Flicker::candle;
  torch.seed = 7;
  large.pos[0] = 4;
  large.flame = true;
  large.fire = ba::Fire::large;
  large.flicker = ba::Flicker::candle;
  large.seed = 9;
  s.lights = {torch, lamp, large};
  return s;
}

}  // namespace

TEST(the_fires_burn_where_their_lights_are) {
  const ba::Scene s = three_lights();
  std::vector<ge::render::Flame> fires;
  ba::bsp_fires(s, 1.0, 2.0, fires);
  CHECK(fires.size() == 2);
  CHECK(fires[0].base[0] == 1 && fires[0].base[1] > 0 && fires[0].seed == 7);
  CHECK(fires[1].base[0] == 4 && fires[1].height_m > fires[0].height_m);
}

TEST(a_fire_burns_as_its_light_flickers) {
  const ba::Scene s = three_lights();
  std::vector<ge::render::Flame> fires;
  for (int i = 0; i < 20; ++i) {
    const double t = i * 0.07;
    ba::bsp_fires(s, t, 2.0, fires);
    const double want = ba::flicker_at(s.lights[0], t) / 0.72;
    CHECK(fires[0].level > 0 && std::abs(fires[0].level - want) < 1e-5);
  }
}
