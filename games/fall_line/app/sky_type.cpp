// Fall Line's sky type off its cvars (app/sky_type.hpp).
//
// ge:summary Fall Line's sky type and knobs from its cvars, as the frame's sky style
// ge:keywords fall-line sky-type sky_style_of
// ge:skill render console
// ge:test tests/fall_line_sky_test.cpp
#include "fall_line/app/sky_type.hpp"

namespace fall_line::app {

std::string sky_style_of(const cfg::Settings& s, ge::render::SkyStyle& out) {
  std::string problem;
  out = ge::render::SkyStyle{};
  if (!ge::render::sky_style_named(s.sky_type, out)) {
    problem = "sv_skytype '" + s.sky_type +
              "' names no sky type; the Earth's is drawn. next: set sv_skytype to one of " +
              std::string(ge::render::sky_type_list()) + ".";
  }
  ge::render::SkyKnobs k;
  k.air = s.sky_air;
  k.dust = s.sky_dust;
  k.sun_size = s.sun_size;
  k.sun2 = s.sun2;
  k.planet = s.planet;
  k.planet_heading = s.planet_dir;
  k.planet_elevation = s.planet_el;
  k.ring = s.ring;
  k.nebula = s.nebula;
  k.stars = s.sky_stars;
  k.glow = s.sky_glow;
  ge::render::with_knobs(out, k);
  return problem;
}

}  // namespace fall_line::app
