// engine/render/sky_style.hpp: the Earth's style is neutral (the frame's rows are the ones the
// sky drew before styles), every type is found by its name in any case, the knobs turn only what
// they are set to, and a new air rebuilds the sky's tables.
//
// ge:summary Tests for the sky types, their knobs, the style's rows and the tables' rebuild
// ge:keywords test render sky sky-type sky-style knobs style_rows same_air prepare_sky
// ge:skill render testing
#include <set>
#include <string>

#include "check.hpp"
#include "render/sky.hpp"
#include "render/sky_style.hpp"

namespace r = ge::render;

TEST(the_earths_style_is_neutral) {
  const double sun[3] = {0, 0.5, 0.8660254};
  float rows[r::kSkyStyleRows][4];
  r::style_rows(r::SkyStyle{}, sun, rows);
  for (int i = 0; i < 3; ++i) {
    CHECK(rows[0][i] == 1.0f);  // Rayleigh
    CHECK(rows[1][i] == 1.0f);  // the dust's absorption
    CHECK(rows[2][i] == 1.0f);  // the sun's tint
  }
  CHECK(rows[0][3] == 1.0f);                                              // the dust
  CHECK(rows[1][3] == 1.0f);                                              // the ozone
  CHECK(rows[2][3] == 1.0f);                                              // the sun's size
  CHECK(rows[3][3] == 0.0f);                                              // no second sun
  CHECK(rows[5][3] == 0.0f);                                              // no planet
  CHECK(rows[7][3] == 0.0f);                                              // no nebula
  CHECK(rows[9][0] == 0.0f && rows[9][1] == 0.0f && rows[9][2] == 0.0f);  // no glow
  CHECK(rows[10][1] == 0.0f);                                             // no stars by day
  // The dust's asymmetry and scale height are the atmosphere's constants (kMieG, kMieHeight).
  CHECK(rows[10][2] == 0.8f);
  CHECK(rows[10][3] == 1.2f);
}

TEST(every_type_is_found_by_its_name) {
  std::set<std::string> names;
  for (const r::SkyType& t : r::sky_types()) {
    names.insert(std::string(t.name));
    r::SkyStyle s;
    CHECK(r::sky_style_named(t.name, s));
    CHECK(!t.caption.empty());
    CHECK(std::string(r::sky_type_list()).find(t.name) != std::string::npos);
  }
  CHECK(names.size() == r::sky_types().size());
  CHECK(r::sky_types()[0].name == "earth");
  r::SkyStyle s;
  CHECK(r::sky_style_named("HELL", s));
  CHECK(s.glow > 0);
  s.glow = 7;
  CHECK(!r::sky_style_named("mordor", s));
  CHECK(s.glow == 7);  // untouched
}

TEST(knobs_turn_only_what_they_are_set_to) {
  r::SkyStyle s;
  CHECK(r::sky_style_named("alien", s));
  const r::SkyStyle before = s;
  r::with_knobs(s, r::SkyKnobs{});
  CHECK(r::same_air(s, before) && s.planet == before.planet && s.sun2 == before.sun2);
  r::SkyKnobs k;
  k.air = 0;
  k.planet = 10;  // degrees
  k.sun2 = 0;
  r::with_knobs(s, k);
  CHECK(s.rayleigh[0] == 0 && s.rayleigh[1] == 0 && s.dust == 0);
  CHECK(s.planet > 0.1745 && s.planet < 0.1746);
  CHECK(s.sun2 == 0);
  CHECK(s.ring == before.ring);
}

TEST(the_second_sun_and_the_planet_stand_off_the_sun) {
  r::SkyStyle s;
  s.sun2 = 1;
  s.sun2_heading = 0.5;
  s.planet = 0.1;
  s.planet_heading = 1.0;
  s.planet_elevation = 0.3;
  const double sun[3] = {0, 0, 1};  // on the horizon, due north (heading 0)
  float rows[r::kSkyStyleRows][4];
  r::style_rows(s, sun, rows);
  CHECK(rows[3][0] > 0.479f && rows[3][0] < 0.480f);  // sin(0.5)
  CHECK(rows[3][1] == 0.0f);
  CHECK(rows[5][1] > 0.2955f && rows[5][1] < 0.2956f);  // sin(0.3)
}

TEST(a_new_air_rebuilds_the_tables) {
  r::Sky sky;
  r::FrameView v;
  r::prepare_sky(sky, v);
  sky.build_tables = sky.build_view = false;
  r::prepare_sky(sky, v);
  CHECK(!sky.build_tables && !sky.build_view);
  // A planet is drawn over the sky: no table moves.
  v.style.planet = 0.2;
  r::prepare_sky(sky, v);
  CHECK(!sky.build_tables && !sky.build_view);
  // A new air: every table.
  CHECK(r::sky_style_named("dust", v.style));
  r::prepare_sky(sky, v);
  CHECK(sky.build_tables && sky.build_view);
  sky.build_tables = sky.build_view = false;
  r::prepare_sky(sky, v);
  CHECK(!sky.build_tables && !sky.build_view);
}
