// games/fall_line/app/lamps.hpp: a racer's headlamp is on the brow of his posed head, lower in
// a tuck, turned with him and tipped down at the snow ahead; it is a shadowed spot and an
// unshadowed flood, nothing by day; the eye adapts to the lamps' light on the snow ahead, nothing
// without lamps, and a dark view is not blinded by one pool in it; a fire and a glowing haze are
// each a lamp, a plain mist none.
//
// ge:summary Tests for Fall Line's lamps: the headlamp's pose and beams, the eye's lamp lux
// ge:keywords test fall-line headlamp headlamp_lights headlamp_of eye_lamp_lux lamp_lux
// ge:keywords glow_lights fire-light
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "fall_line/app/lamps.hpp"
#include "render/flame.hpp"
#include "render/fog.hpp"
#include "render/lights.hpp"
#include "render/view.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fa = fall_line::app;
namespace r = ge::render;

ge::skier::SkierState skier_at(const ge::mountain::mapgen::Level& level, double x, double z,
                               double heading) {
  ge::skier::SkierState s;
  s.x = x;
  s.z = z;
  s.y = level.ground_at(x, z);
  s.heading = heading;
  return s;
}

fa::Headlamp lamp_on(const ge::mountain::mapgen::Level& level, const ge::skier::SkierState& s) {
  ge::skier::RigPoser poser;
  ge::skier::RigPose pose;
  poser.pose(s, 0, pose);
  return fa::headlamp_of(ge::skier::figure_frame(s, ge::skier::World{&level, nullptr}), pose);
}

}  // namespace

TEST(the_headlamp_is_on_the_brow_of_the_posed_head_aimed_ahead_and_down) {
  const ge::mountain::mapgen::Level level = ge::skier::synthetic_slope();
  ge::skier::SkierState s = skier_at(level, 300, 200, 0);
  const fa::Headlamp lamp = lamp_on(level, s);
  // Over the boots at a skier's brow, a little forward, aimed down the skis and down at the snow.
  CHECK(lamp.at[1] - s.y > 1.2 && lamp.at[1] - s.y < 1.9);
  CHECK(std::fabs(lamp.at[0] - s.x) < 0.3 && lamp.at[2] > s.z);
  CHECK(lamp.dir[2] > 0.8f && lamp.dir[1] < -0.1f);
  // Lower in a tuck.
  s.tuck = 1;
  CHECK(lamp_on(level, s).at[1] < lamp.at[1] - 0.2);
  // Turned with him.
  s.tuck = 0;
  s.heading = 0.5 * 3.14159265358979323846;
  CHECK(lamp_on(level, s).dir[0] > 0.85f);
}

TEST(the_headlamp_is_a_shadowed_spot_and_a_flood_and_nothing_by_day) {
  const fa::Headlamp lamp;
  std::vector<r::LocalLight> out;
  fa::headlamp_lights(lamp, 77, 0, true, out);
  CHECK(out.empty());
  fa::headlamp_lights(lamp, 77, 1, true, out);
  CHECK(out.size() == 2);
  CHECK(out[0].shadow == 77);
  CHECK(out[1].shadow == 0);
  CHECK(out[0].outer_deg < out[1].outer_deg);
  CHECK(out[0].candela > 10 * out[1].candela);
  out.clear();
  fa::headlamp_lights(lamp, 77, 1, false, out);
  CHECK(out[0].shadow == 0);
}

TEST(the_eye_adapts_to_the_lamps_on_the_snow_ahead) {
  const ge::mountain::mapgen::Level level = ge::skier::synthetic_slope();
  r::FrameView v;
  v.eye[0] = 300;
  v.eye[2] = 200;
  v.eye[1] = level.ground_at(300, 200) + 2;
  v.heading = 0;
  CHECK(fa::eye_lamp_lux({}, v, level) == 0);
  // A street lamp over the snow 10 m ahead: lit, but the dark beyond holds the eye under it.
  r::LocalLight l;
  l.position[0] = 300;
  l.position[2] = 210;
  l.position[1] = level.ground_at(300, 210) + 5;
  l.candela = 1000;
  l.range_m = 22;
  const r::LocalLight one[1] = {l};
  const double lux = fa::eye_lamp_lux(one, v, level);
  const double under[3] = {300, level.ground_at(300, 210) + 0.1, 210};
  CHECK(lux > 0);
  CHECK(lux < r::lamp_lux_at(one, under, 0));
  // A row of them down the view: the eye takes them all in.
  std::vector<r::LocalLight> row;
  for (int k = 0; k < 8; ++k) {
    r::LocalLight m = l;
    m.position[2] = 200 + 5 * k;
    m.position[1] = level.ground_at(300, m.position[2]) + 5;
    row.push_back(m);
  }
  CHECK(fa::eye_lamp_lux(row, v, level) > 3 * lux);
}

TEST(a_fire_and_a_glowing_haze_are_each_a_lamp_and_a_plain_mist_none) {
  const ge::mountain::mapgen::Level level = ge::skier::synthetic_slope();
  ge::render::FogVolume mist;
  ge::render::FogVolume haze;
  haze.glow[0] = 0.9;
  haze.glow[1] = 0.3;
  haze.hug_ground = true;
  haze.centre[0] = 300;
  haze.centre[2] = 200;
  const double base[3] = {300, level.ground_at(300, 200), 200};
  const ge::render::Flame fire = ge::render::campfire_flame(base, 3);
  std::vector<ge::render::LocalLight> out;
  const ge::render::FogVolume fog[] = {mist, haze};
  fa::glow_lights(level, fog, std::span<const ge::render::Flame>(&fire, 1), out);
  CHECK(out.size() == 2);
  CHECK(out[0].candela == ge::render::flame_light(fire).candela);
  CHECK(out[1].candela > 0);
  CHECK(std::fabs(out[1].position[1] - (level.ground_at(300, 200) + haze.half[1])) < 1e-9);
}
