// Fall Line's sky type off its cvars (app/sky_type.hpp).
//
// ge:summary Fall Line's sky type and knobs from its cvars, as the frame's sky style
// ge:keywords fall-line sky-type sky_style_of sv_moons moon_set_of
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

std::string moon_set_of(const cfg::Settings& s, ge::render::MoonSet& out) {
  const std::string_view words =
      s.moons.empty() || s.moons == "type" ? ge::render::sky_moons_named(s.sky_type) : s.moons;
  std::string problem;
  if (ge::render::moon_set_named(words, out, problem)) return {};
  out = ge::render::MoonSet{};
  return "sv_moons: " + problem + "; the Earth's moon is drawn. next: set sv_moons to names " +
         "such as `phobos deimos` or `moon io:2`, or type for the sky type's own.";
}

std::string cloud_stack_of(const cfg::Settings& s, FrameOptions& o) {
  o.cloud_count = -1;
  if (s.cloud_kinds.empty() || s.cloud_kinds == "map") return {};
  const std::string problem =
      ge::render::cloud_stack_named(s.cloud_kinds, o.cloud_kinds, o.cloud_count);
  if (problem.empty()) return {};
  return "sv_clouds: " + problem + ". next: set sv_clouds to kinds joined by '+' (such as " +
         "cumulus+cirrus), or map for the map's own.";
}

}  // namespace fall_line::app
