// plugins/brush/app/bsp_sky.hpp: a classic map's sky texture picks our sky by its colours. A dark
// red-brown smoke is an ash sky dimmed against the rooms, a lavender dusk a nebula's, a green haze
// a toxic one, a map with no sky texture the Earth's fair day; the dimmer the texture, the dimmer
// the sky; the tint leans toward the texture's hue; a type named over the pick keeps its hour. The
// classic night: every map an evening or a night of its own with a moon up, windy, scud racing
// low under a broken deck, the eye adapted to the rooms.
//
// ge:summary Tests for the sky picked by a classic map's sky texture
// ge:keywords test brush bsp sky pick_sky sky-texture hue tint sky-light with_type night_over
// ge:keywords classic-night scud clouds_of dress_sky
// ge:skill brush testing
#include <string>

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

TEST(a_classic_map_is_never_seen_by_day) {
  for (const char* map :
       {"start", "e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6", "e1m7", "e1m8"}) {
    ba::SkyPick p = ba::pick_sky(colours(0.1, 0.1, 0.22, 0.25));
    const std::string type = p.type;
    ba::night_over(p, map);
    CHECK(p.night);
    CHECK(p.type == type);
    CHECK(p.sun_elevation <= -3);  // an evening's or a night's, never a day's
    CHECK(p.moon_elevation >= 8);  // a moon up for the cloud to cross
    CHECK(p.wind >= 8);            // always windy
    CHECK(p.scud_cover >= 0.3 && p.deck_cover >= 0.4 && p.deck_cover < 0.6);  // partly cloudy
    CHECK(p.adapt_cd > 0);
  }
}

TEST(a_maps_night_is_its_own_and_always_the_same) {
  ba::SkyPick a = ba::pick_sky(ba::SkyColours{}), b = a, c = a;
  ba::night_over(a, "e1m1");
  ba::night_over(b, "e1m1");
  ba::night_over(c, "e1m2");
  CHECK(a.sun_elevation == b.sun_elevation && a.wind == b.wind && a.seed == b.seed);
  CHECK(a.seed != c.seed);
}

TEST(the_night_scud_races_low_under_the_deck) {
  ba::SkyPick p = ba::pick_sky(ba::SkyColours{});
  ba::night_over(p, "e1m1");
  const double top = 50;
  const ge::render::Clouds c0 = ba::clouds_of(p, top, 0);
  const ge::render::Clouds c1 = ba::clouds_of(p, top, 60);
  CHECK(c0.count == 2);
  const auto& scud = c0.layers[0];
  const auto& deck = c0.layers[1];
  CHECK(scud.base_m >= top + 200 && scud.top_m < deck.base_m);
  CHECK(deck.base_m >= top + 900);
  // Across the sky: the angle a minute's drift sweeps overhead, the scud's many times the deck's.
  const double scud_sweep = (c1.layers[0].drift_m - scud.drift_m) / (scud.base_m - top);
  const double deck_sweep = (c1.layers[1].drift_m - deck.drift_m) / (deck.base_m - top);
  CHECK(scud_sweep > 2 * deck_sweep);
  CHECK(c1.layers[0].drift_m - scud.drift_m > 60 * p.wind);
}

TEST(a_night_dressed_keys_the_moon_and_keeps_the_rooms_light) {
  ba::SkyPick p = ba::pick_sky(ba::SkyColours{});
  ba::night_over(p, "e1m3");
  ge::render::FrameView v;
  ba::dress_sky(v, p, 0, 10);
  CHECK(v.clouds.count == 2);
  CHECK(v.adapt_cd == p.adapt_cd);
  CHECK(v.time_s == 10);
  CHECK(v.night.moon_elevation > 0);
  // By day nothing of the night is set.
  ba::SkyPick day = ba::pick_sky(ba::SkyColours{});
  ge::render::FrameView dv;
  ba::dress_sky(dv, day, 0, 10);
  CHECK(dv.adapt_cd == 0 && dv.clouds.count == 1 && !dv.night.moon_key);
}
