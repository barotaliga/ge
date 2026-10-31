// plugins/brush/app/bsp_flames.hpp: a wall torch's and a large flame's light entities become
// flames over their origins, a plain lamp none, and each fire's level follows its light's flicker;
// as bonfires a large flame burns twice as tall in five tongues and throws sparks up its wind, and
// a large one in a lantern's cage burns bigger with no sparks, and a wall torch and a small flame
// stay as they were.
//
// ge:summary Tests for the map's fires: which lights burn, where, and their level
// ge:keywords test brush bsp torch flame fire bsp_fires flicker-sync bonfire bonfire_sparks
// ge:skill brush testing
#include <cmath>
#include <vector>

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

TEST(a_large_flame_burns_as_a_bonfire_and_a_wall_torch_stays_a_torch) {
  const ba::Scene s = three_lights();
  std::vector<ge::render::Flame> plain, bon;
  ba::bsp_fires(s, 1.0, 2.0, plain);
  ba::FireLook look;
  look.bonfires = true;
  ba::bsp_fires(s, 1.0, 2.0, bon, look);
  CHECK(bon.size() == 6);  // the torch, then the bonfire's body and its four tongues
  CHECK(bon[0].height_m == plain[0].height_m && bon[0].radius_m == plain[0].radius_m);
  CHECK(bon[1].height_m > 1.9f * plain[1].height_m && bon[1].radius_m > 2 * plain[1].radius_m);
  for (int k = 2; k < 6; ++k) {
    CHECK(bon[k].height_m < bon[1].height_m);
    CHECK(std::abs(std::hypot(bon[k].base[0] - 4, bon[k].base[2]) - ba::kBonfireBed) < 1e-9);
  }
  // Its light shines from higher up and softer.
  CHECK(ba::fire_light_lift(s.lights[2], true) > ba::fire_light_lift(s.lights[2]));
  CHECK(ba::fire_light_size(s.lights[2], true) > ba::fire_light_size(s.lights[2]));
  CHECK(ba::fire_light_lift(s.lights[0], true) == ba::fire_light_lift(s.lights[0]));
}

TEST(a_bonfire_throws_sparks_that_go_with_the_wind_where_the_sky_is_open) {
  ba::Scene s = three_lights();
  ba::FireLook look;
  std::vector<ge::render::Particle> none;
  ba::bonfire_sparks(s, 0, 4, look, none);
  CHECK(none.empty());  // no bonfires, no sparks
  look.bonfires = true;
  look.draught = 0;
  look.air[0] = 6;
  std::vector<ge::render::Particle> in, out;
  ba::bonfire_sparks(s, 0, 4, look, in);  // under a roof: the wind does not reach it
  s.lights[2].sky = 1;
  ba::bonfire_sparks(s, 0, 4, look, out);  // under the open sky
  CHECK(in.size() == out.size() && in.size() > 100);
  double rise = 0, drift_in = 0, drift_out = 0;
  for (size_t i = 0; i < in.size(); ++i) {
    CHECK(std::abs(in[i].p[0] - 4) < 3 && in[i].kelvin > 0);  // over the bonfire, not the torch
    rise += in[i].p[1] - s.lights[2].pos[1];
    drift_in += in[i].p[0] - 4;
    drift_out += out[i].p[0] - 4;
  }
  const double n = static_cast<double>(in.size());
  CHECK(rise / n > 0.5);
  CHECK(std::abs(drift_in / n) < 0.2 && drift_out / n > 1);
}

TEST(a_large_flame_in_a_lanterns_cage_burns_bigger_but_no_bonfire) {
  // A map with no tree (a synthetic scene) places nothing: every large flame keeps its room.
  ba::Scene s = three_lights();
  ba::place_fires(s);
  CHECK(s.lights[2].fire == ba::Fire::large && s.lights[2].sky == 0);
  std::vector<ge::render::Flame> plain, lit;
  s.lights[2].fire = ba::Fire::lantern;
  ba::bsp_fires(s, 1.0, 2.0, plain);
  ba::FireLook look;
  look.bonfires = true;
  ba::bsp_fires(s, 1.0, 2.0, lit, look);
  // One flame, taller and wider than the pack's large flame and smaller than a bonfire.
  CHECK(plain.size() == 2 && lit.size() == 2);
  CHECK(lit[1].height_m > 1.3f * plain[1].height_m && lit[1].height_m < 1.6f);
  CHECK(lit[1].radius_m > 1.4f * plain[1].radius_m);
  CHECK(ba::fire_light_gain(s.lights[2], true) > 1 &&
        ba::fire_light_gain(s.lights[2], true) < ba::kBonfireLight);
  std::vector<ge::render::Particle> sparks;
  ba::bonfire_sparks(s, 0, 2, look, sparks);
  CHECK(sparks.empty());
  // A small flame is drawn as a large one's either way, its light where it was.
  s.lights[2].fire = ba::Fire::flame;
  ba::bsp_fires(s, 1.0, 2.0, lit, look);
  CHECK(lit[1].height_m == plain[1].height_m);
  CHECK(ba::fire_light_lift(s.lights[2], true) == ba::fire_light_lift(s.lights[2]));
}
