// plugins/brush/app/bsp_sky.hpp: a classic map's sky texture picks our sky by its colours. A dark
// red-brown smoke is an ash sky dimmed against the rooms, a lavender dusk a nebula's, a green haze
// a toxic one, a map with no sky texture the Earth's fair day; the dimmer the texture, the dimmer
// the sky; the tint leans toward the texture's hue; a type named over the pick keeps its hour.
//
// ge:summary Tests for the sky picked by a classic map's sky texture
// ge:keywords test brush bsp sky pick_sky sky-texture hue tint sky-light with_type
// ge:skill brush testing
#include "brush/app/bsp_sky.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;

ba::SkyColours colours(double r, double g, double b, double cover) {
  ba::SkyColours c;
  c.back[0] = c.cloud[0] = r;
  c.back[1] = c.cloud[1] = g;
  c.back[2] = c.cloud[2] = b;
  c.cover = cover;
  c.found = true;
  return c;
}

}  // namespace

TEST(no_sky_texture_is_the_earths_fair_day) {
  const ba::SkyPick p = ba::pick_sky(ba::SkyColours{});
  CHECK(p.type == "earth");
  CHECK(p.sky_light == 1.0);
}

TEST(a_dark_red_brown_smoke_is_an_ash_sky_dimmed) {
  const ba::SkyPick p = ba::pick_sky(colours(0.012, 0.006, 0.005, 0.6));
  CHECK(p.type == "ash");
  CHECK(p.sky_light < 0.4);
  CHECK(p.tint[0] == 1.0);
  CHECK(p.tint[2] < p.tint[0]);
  CHECK(p.cover > 0.5);
}

TEST(a_lavender_dusk_is_a_nebulas_sky) {
  const ba::SkyPick p = ba::pick_sky(colours(0.1, 0.1, 0.22, 0.25));
  CHECK(p.type == "nebula");
  CHECK(p.sky_light > ba::pick_sky(colours(0.012, 0.006, 0.005, 0.6)).sky_light);
}

TEST(a_green_haze_is_a_toxic_sky) {
  CHECK(ba::pick_sky(colours(0.1, 0.3, 0.05, 0.3)).type == "toxic");
}

TEST(a_type_named_over_the_pick_keeps_its_hour) {
  ba::SkyPick p = ba::pick_sky(colours(0.012, 0.006, 0.005, 0.6));
  const double hour = p.sun_elevation;
  CHECK(ba::with_type(p, "hell"));
  CHECK(p.type == "hell");
  CHECK(p.sun_elevation == hour);
  CHECK(!ba::with_type(p, "no-such-sky"));
  CHECK(p.type == "hell");
}
