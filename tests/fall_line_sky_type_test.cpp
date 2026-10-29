// games/fall_line/app/sky_type.hpp: sv_skytype and its knobs reach the frame's sky style through
// the console, and an unknown type keeps the Earth's with a problem that names the types; r_sunsize
// and r_moonsize reach the frame's options.
//
// ge:summary Tests for Fall Line's sky type cvars reaching the sky style
// ge:keywords test fall-line sky-type sv_skytype sky-knobs sky_style_of r_sunsize r_moonsize
// ge:skill render console testing
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/sky_type.hpp"
#include "fall_line/cfg/cvars.hpp"

namespace {

fall_line::cfg::Settings settings_after(const std::string& script) {
  ge::console::Console console;
  fall_line::cfg::Settings s;
  fall_line::cfg::add_cvars(console, s);
  console.execute(script);
  return s;
}

}  // namespace

TEST(the_earth_by_default) {
  const auto s = settings_after("");
  ge::render::SkyStyle style;
  style.glow = 3;
  CHECK(fall_line::app::sky_style_of(s, style).empty());
  CHECK(style.glow == 0 && style.planet == 0 && style.rayleigh[2] == 1);
}

TEST(a_type_and_its_knobs_reach_the_style) {
  const auto s = settings_after("sv_skytype void; sv_planet 30; sv_ring 0; sv_skystars 0.5");
  ge::render::SkyStyle style;
  CHECK(fall_line::app::sky_style_of(s, style).empty());
  CHECK(style.rayleigh[0] == 0 && style.dust == 0);  // the void's air
  CHECK(style.planet > 0.523 && style.planet < 0.524);
  CHECK(style.ring == 0);
  CHECK(style.stars == 0.5);
  CHECK(style.nebula > 0);  // the type's own
}

TEST(an_unknown_type_keeps_the_earth_and_says_so) {
  const auto s = settings_after("sv_skytype mordor");
  ge::render::SkyStyle style;
  const std::string problem = fall_line::app::sky_style_of(s, style);
  CHECK(problem.find("mordor") != std::string::npos);
  CHECK(problem.find("next:") != std::string::npos);
  CHECK(problem.find("hell") != std::string::npos);
  CHECK(style.rayleigh[0] == 1);
}

TEST(the_discs_are_drawn_larger_by_their_cvars) {
  ge::console::Console c;
  fall_line::cfg::Settings s;
  fall_line::cfg::add_cvars(c, s);
  CHECK(s.disc_sun == 1 && s.disc_moon == 1);
  c.execute("r_sunsize 3; r_moonsize 5");
  c.cvars().apply_latched();
  CHECK(c.problems().empty());
  const fall_line::app::FrameOptions o = fall_line::app::frame_options(s);
  CHECK(o.sun_size == 3 && o.moon_size == 5);
  // Held to 0.5 .. 20.
  c.execute("r_moonsize 100");
  c.cvars().apply_latched();
  CHECK(s.disc_moon <= 20);
}
