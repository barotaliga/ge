// The definitions behind shared.hpp.
//
// ge:summary The shared checks' bodies: R13, R15 and R19
// ge:keywords analysis
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/analysis/shared.hpp"

#include <cmath>
#include <string>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/solar.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/sun.hpp"
#include "mountain/mapgen/weather.hpp"

namespace ge::mountain::analysis {

namespace R = mapgen::rules;
using ge::core::angle_diff;
using ge::core::hypot;
using ge::core::kPi;
using mapgen::Band;
using mapgen::TrackPoint;
using mapgen::within_band;

void check_start_line(const mapgen::Level& level, const Add& add) {
  const auto& pts = level.track.points;
  if (level.grid.size() != size_t(R::grid::slots)) {
    add("R13", Severity::error, str(double(level.grid.size())) + " start slots");
  }
  for (size_t i = 0; i <= level.grid.size(); i++) {
    const mapgen::Spawn& g = i == 0 ? level.spawn : level.grid[i - 1];
    const mapgen::TrackHit hit = mapgen::nearest_track_point(level.track, g.x, g.z);
    const TrackPoint& p = pts[size_t(hit.index)];
    const std::string who = i == 0 ? "the spawn" : "start slot " + str(double(i - 1));
    if (hit.distance > p.width / 2 - 1) add("R13", Severity::error, who + " stands off the piste");
    if (level.packed_at(g.x, g.z) < 0.98) {
      add("R13", Severity::error, who + " is not on packed snow");
    }
    if (hit.s > R::track::step) {
      add("R13", Severity::error, who + " stands " + fmt(hit.s) + " m down the piste");
    }
    if (std::abs(angle_diff(g.heading, p.heading)) > 0.05) {
      add("R13", Severity::error, who + " does not face down the piste");
    }
  }
  if (mapgen::nearest_track_point(level.track, level.spawn.x, level.spawn.z).distance > 0.1) {
    add("R13", Severity::error, "the spawn is off the centreline");
  }
  for (size_t a = 0; a < level.grid.size(); a++) {
    for (size_t b = a + 1; b < level.grid.size(); b++) {
      const double d = hypot(level.grid[a].x - level.grid[b].x, level.grid[a].z - level.grid[b].z);
      if (d < R::grid::spacing - 0.05) {
        add("R13", Severity::error,
            "start slots " + str(double(a)) + " and " + str(double(b)) + " stand " + fmt(d) +
                " m apart");
      }
    }
  }
}

void check_day(const mapgen::Level& level, const Add& add) {
  const mapgen::Region& region = mapgen::region_row(level.region);
  // R15 — the day, from the region's own bands; an evening map starts from sunset instead.
  const mapgen::Sun& day = level.sun;
  const ge::core::SunPlace sun =
      ge::core::sun_at(day.hour, day.latitude, mapgen::declination_of(day.day_of_year));
  const double elevation = (sun.elevation * 180) / kPi;
  const mapgen::Weather& weather = level.weather;
  const bool start_ok =
      weather.evening
          ? within_band(day.hour - mapgen::sunset_of(day.day_of_year, day.latitude),
                        R::sun::evening, 1e-3)
          : within_band(day.hour, R::sun::hour) && elevation >= R::sun::min_elevation - 0.05;
  if (!start_ok || !within_band(day.latitude, region.sun.latitude) ||
      !within_band(day.day_of_year, region.sun.day_of_year)) {
    add("R15", Severity::error,
        "the sun: " + fmt(day.hour) + " h on day " + str(day.day_of_year) + " at " +
            fmt(day.latitude) + "°N, " + fmt(elevation) + "° up");
  }
  if (day.facing) {
    const double off = std::abs(angle_diff(*day.facing, sun.azimuth));
    if (off > R::sun::facing + 1e-6) {
      add("R15", Severity::error,
          "the sun stands " + fmt((off * 180) / kPi, 0) + "° off the face's bearing");
    }
  }

  // R19 — the weather: a sky the rule deals, at numbers inside its bands.
  {
    const Band wind = R::weather::wind[size_t(weather.kind)];
    Band fall{0, 0};
    if (weather.kind == mapgen::WeatherKind::flurries) fall = R::weather::snowfall_flurries;
    if (weather.kind == mapgen::WeatherKind::snow) fall = R::weather::snowfall_snow;
    if (weather.kind == mapgen::WeatherKind::storm) fall = R::weather::snowfall_storm;
    const bool bad =
        !within_band(weather.wind, wind, 1e-6) ||
        (mapgen::snows(weather.kind) ? !within_band(weather.snowfall, fall)
                                     : weather.snowfall != 0) ||
        (weather.kind == mapgen::WeatherKind::fog ? !within_band(weather.fog, R::weather::fog)
                                                  : weather.fog != 0);
    if (bad) {
      add("R19", Severity::error,
          std::string("a ") + mapgen::weather_name(weather.kind) + " sky: wind " +
              fmt(weather.wind) + " m/s, fall " + fmt(weather.snowfall) + ", fog " +
              fmt(weather.fog));
    }
  }
}

}  // namespace ge::mountain::analysis
