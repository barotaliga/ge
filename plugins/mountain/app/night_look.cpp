// The map's moon on its date (night_look.hpp says how the date and the distance are read).
//
// ge:summary The map's moon on its date at an hour and the night it lays into a frame
// ge:keywords night moon date night_look moon_of
// ge:skill mountain render
// ge:test tests/mountain_night_look_test.cpp
#include "mountain/app/night_look.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/solar.hpp"
#include "render/night.hpp"

namespace ge::mountain::app {

namespace {

constexpr double kEarthKm = 6371;

}  // namespace

MapMoon moon_of(const mapgen::Level& level, double hour) {
  const double lat = level.sun.latitude;
  const double jd0 = core::julian_day_of_year(kSkyYear, level.sun.day_of_year);
  const core::DatedMoonPlace m = core::moon_on_date(hour, lat, 0.0, jd0);
  const double jd = jd0 + core::universal_hour_of(jd0, hour, 0.0) / 24.0;
  const double geo = core::lunar_coordinates(jd).distance * kEarthKm;
  const double s = core::libm::sin(std::max(m.elevation, -0.3));
  const double km = std::sqrt(geo * geo - 2 * geo * kEarthKm * s + kEarthKm * kEarthKm);
  return {m.elevation, m.azimuth, m.lit, km, core::sidereal_time(jd)};
}

void night_look(const mapgen::Level& level, double hour, render::FrameView& view) {
  const MapMoon m = moon_of(level, hour);
  render::set_night(view, {m.elevation, m.azimuth, m.lit, m.distance_km}, level.sun.latitude,
                    m.sidereal_deg);
}

}  // namespace ge::mountain::app
