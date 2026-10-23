// The map's moon on its date (night_look.hpp says how the date and the distance are read).
//
// ge:summary The map's moon on its date at an hour and the night it lays into a frame
// ge:keywords night moon date night_look moon_of
// ge:skill mountain render
// ge:test tests/mountain_night_look_test.cpp
#include "mountain/app/night_look.hpp"

#include "core/solar.hpp"
#include "render/night.hpp"

namespace ge::mountain::app {

MapMoon moon_of(const mapgen::Level& level, double hour) {
  const double jd0 = core::julian_day_of_year(kSkyYear, level.sun.day_of_year);
  const core::MoonSky m = core::moon_on_date(hour, level.sun.latitude, 0.0, jd0);
  const double jd = jd0 + core::universal_hour_of(jd0, hour, 0.0) / 24.0;
  return {
      m.apparent_elevation, m.azimuth, m.lit, m.distance_km, core::apparent_sidereal_time(jd), m};
}

void night_look(const mapgen::Level& level, double hour, render::FrameView& view) {
  const MapMoon m = moon_of(level, hour);
  render::set_night(view, render::moon_of(m.sky), level.sun.latitude, m.sidereal_deg);
}

}  // namespace ge::mountain::app
