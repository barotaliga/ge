// plugins/mountain/app/night_look.hpp: a map's moon is the same on the same day of the year; a
// month on, the phase has come round; its distance is a moon's; and at midnight under a full
// moon the night is laid with the moon as the key light.
//
// ge:summary Tests for the map's moon: a date's moon, its phase and distance, the night laid
// ge:keywords test night moon night_look moon_of phase date
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
#include "mountain/app/night_look.hpp"
#include "render/night.hpp"

namespace {

namespace mapp = ge::mountain::app;

ge::mountain::mapgen::Level level_on(double day) {
  ge::mountain::mapgen::Level level{};
  level.sun.hour = 12;
  level.sun.day_of_year = day;
  level.sun.latitude = 46;
  return level;
}

}  // namespace

TEST(a_day_of_the_year_is_always_the_same_moon) {
  const mapp::MapMoon a = mapp::moon_of(level_on(30), 22);
  const mapp::MapMoon b = mapp::moon_of(level_on(30), 22);
  CHECK(a.elevation == b.elevation && a.lit == b.lit && a.distance_km == b.distance_km);
  CHECK(a.lit >= 0 && a.lit <= 1);
  CHECK(a.distance_km > 350000 && a.distance_km < 410000);
  CHECK(a.sidereal_deg >= 0 && a.sidereal_deg < 360);
}

TEST(the_phase_comes_round_in_a_month) {
  // A synodic month is 29.53 days: the lit share a month on is the same within a few hundredths,
  // and a full moon's half a month on is near new.
  double full = 0, day = 0;
  for (int d = 1; d <= 30; ++d) {
    const double lit = mapp::moon_of(level_on(d), 0).lit;
    if (lit > full) full = lit, day = d;
  }
  const double month = mapp::moon_of(level_on(day + 29.53), 0).lit;
  const double half = mapp::moon_of(level_on(day + 14.77), 0).lit;
  CHECK(std::fabs(full - month) < 0.05);
  CHECK(half < 0.1);
}

TEST(a_full_moon_at_midnight_keys_the_night) {
  // Find the fullest night of a month and lay it.
  double best = 0, day = 0;
  for (int d = 1; d <= 30; ++d) {
    const double lit = mapp::moon_of(level_on(d), 0).lit;
    if (lit > best) best = lit, day = d;
  }
  CHECK(best > 0.95);
  const ge::mountain::mapgen::Level level = level_on(day);
  ge::render::FrameView v;
  v.sun_elevation = -0.6;
  mapp::night_look(level, 0, v);
  CHECK(v.night.moon_key);
  CHECK(v.night.moon_illuminance * ge::render::kSunLux > 0.25);
  CHECK(v.sun_elevation == v.night.moon_elevation);
}
