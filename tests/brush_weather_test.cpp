// plugins/brush/app/bsp_weather.hpp: the weather over a BSP map. Its cvars are registered with
// their tiers and ranges and write the values it reads; off, nothing falls and nothing is wet; a
// rain wets the ground and fills the puddles from its first frame, a dry spell after it dries them
// a frame at a time; snow settles and rain does not; the fall slants downwind, the more the harder
// the wind; a storm flashes and a drizzle never does; the frame carries it all to the view and to
// the renderer's options, and the player's switches take each part away.
//
// ge:summary Tests for the weather over a BSP map: cvars, wet, snow, slant, lightning, the frame
// ge:keywords test brush bsp weather cvars rain snow wet puddles slant lightning apply_weather
// ge:skill brush testing
#include <cmath>

#include "brush/app/bsp_weather.hpp"
#include "check.hpp"
#include "console/cvar.hpp"

namespace {

namespace ba = ge::brush::app;
namespace con = ge::console;

constexpr double kEye[3] = {0, 1.6, 0};

ba::WeatherCvars rain(double precip, double wind) {
  ba::WeatherCvars c;
  c.on = true;
  c.precip = precip;
  c.rain = 1;
  c.wind = wind;
  c.gust = 0;
  return c;
}

}  // namespace

TEST(the_cvars_are_registered_with_their_tiers_and_write_the_values) {
  con::Cvars cvars;
  ba::WeatherCvars c;
  ba::add_weather_cvars(cvars, c);
  for (const char* name :
       {"sv_weather", "sv_precip", "sv_rain", "sv_wind", "sv_winddir", "sv_gust", "r_precip",
        "r_precip_density", "r_splashes", "r_wet", "r_snowcover", "r_lightning"}) {
    CHECK(cvars.find(name) != nullptr);
  }
  CHECK((cvars.find("sv_precip")->flags & con::kServer) != 0);
  CHECK((cvars.find("r_wet")->flags & con::kArchive) != 0);
  CHECK(!c.on);
  cvars.set("sv_weather", "1", con::Source::local);
  cvars.set("sv_precip", "0.8", con::Source::local);
  cvars.set("sv_rain", "3", con::Source::local);  // clamped to its range
  cvars.set("r_lightning", "0", con::Source::local);
  CHECK(c.on);
  CHECK(std::fabs((c.precip) - (0.8)) < 1e-12);
  CHECK(std::fabs((c.rain) - (1.0)) < 1e-12);
  CHECK(!c.lightning);
}

TEST(off_nothing_falls_and_nothing_is_wet) {
  ba::WeatherCvars c = rain(0.8, 10);
  c.on = false;
  ba::WeatherState s;
  const ba::WeatherFrame w = ba::step_weather(c, s, kEye, 10, 0.2);
  CHECK(!w.on);
  CHECK(w.draw.fall == 0);
  ge::render::FrameView view;
  ba::Options o;
  o.weather = true;
  ba::apply_weather(w, c, view, o);
  CHECK(!o.weather);
  CHECK(view.weather.wet == 0);
  CHECK(view.weather.puddles == 0);
  CHECK(view.weather.snow == 0);
}

TEST(rain_wets_from_its_first_frame_and_a_dry_spell_dries_it) {
  ba::WeatherState s;
  const ba::WeatherFrame w = ba::step_weather(rain(0.6, 4), s, kEye, 100, 0.2);
  CHECK(w.wet.wet > 0.99);
  CHECK(w.wet.puddles > 0);
  CHECK(w.cover.cover < 1e-9);  // rain lays no snow
  ba::WeatherCvars dry = rain(0, 4);
  double last = w.wet.water;
  for (int k = 1; k <= 10; ++k) {
    const ba::WeatherFrame d = ba::step_weather(dry, s, kEye, 100 + 60.0 * k, 0.5);
    CHECK(d.wet.water < last);
    last = d.wet.water;
  }
}

TEST(snow_settles_and_the_fall_slants_downwind_the_more_the_harder_the_wind) {
  ba::WeatherCvars snow = rain(0.5, 2);
  snow.rain = 0;
  ba::WeatherState s;
  CHECK(ba::step_weather(snow, s, kEye, 10, 0).cover.cover > 0.3);
  const auto slant = [](const ba::WeatherFrame& w) {
    return std::hypot(w.way[0], w.way[2]) / -w.way[1];
  };
  ba::WeatherCvars c = rain(0.5, 2);
  ba::WeatherState s1, s2, s3;
  const ba::WeatherFrame calm = ba::step_weather(c, s1, kEye, 10, 0);
  CHECK(calm.way[1] < 0);
  c.wind = 8;
  const ba::WeatherFrame breeze = ba::step_weather(c, s2, kEye, 10, 0);
  CHECK(slant(breeze) > 2 * slant(calm));
  // A gale's fall is kept within 75 degrees of the vertical: it still comes from the sky.
  c.wind = 40;
  const ba::WeatherFrame gale = ba::step_weather(c, s3, kEye, 10, 0);
  CHECK(slant(gale) > slant(breeze));
  CHECK(slant(gale) < 3.74);
  // The wind blows FROM the bearing: from 0 (+z) the fall moves toward -z.
  c.wind_from = 0;
  ba::WeatherState s4;
  CHECK(ba::step_weather(c, s4, kEye, 10, 0).way[2] < 0);
}

TEST(a_storm_flashes_and_a_drizzle_never_does) {
  const ba::WeatherCvars storm = rain(0.95, 18);
  const ba::WeatherCvars drizzle = rain(0.15, 2);
  double brightest = 0, drizzle_light = 0;
  ba::WeatherState a, b;
  for (int k = 0; k < 6000; ++k) {
    const double t = 0.05 * k;
    brightest = std::fmax(brightest, ba::step_weather(storm, a, kEye, t, 0).flash);
    drizzle_light = std::fmax(drizzle_light, ba::step_weather(drizzle, b, kEye, t, 0).flash);
  }
  CHECK(brightest > 0.5);
  CHECK(drizzle_light == 0);
  ba::WeatherCvars off = storm;
  off.lightning = false;
  ba::WeatherState c;
  for (int k = 0; k < 600; ++k) CHECK(ba::step_weather(off, c, kEye, 0.05 * k, 0).flash == 0);
}

TEST(the_frame_carries_the_weather_and_the_switches_take_parts_away) {
  ba::WeatherCvars c = rain(0.7, 6);
  ba::WeatherState s;
  const ba::WeatherFrame w = ba::step_weather(c, s, kEye, 30, 0.2);
  ge::render::FrameView view;
  ba::Options o;
  ba::apply_weather(w, c, view, o);
  CHECK(o.weather);
  CHECK(o.raining > 0.5);
  CHECK(std::fabs((o.fall[0]) - (w.way[0])) < 1e-12);
  CHECK(view.weather.wet > 0.99);
  CHECK(view.weather.visibility_m > 0);
  CHECK(view.weather.visibility_m < 20000);
  CHECK(view.weather.sun_pass < 1);
  CHECK(w.draw.fall > 0);
  CHECK(w.draw.splash > 0);
  c.wet = false;
  c.splashes = false;
  c.draw = false;
  ba::WeatherState s2;
  const ba::WeatherFrame bare = ba::step_weather(c, s2, kEye, 30, 0.2);
  ge::render::FrameView v2;
  ba::apply_weather(bare, c, v2, o);
  CHECK(v2.weather.wet == 0);
  CHECK(v2.weather.puddles == 0);
  CHECK(bare.draw.fall == 0);
  CHECK(bare.draw.splash == 0);
}

TEST(the_particles_follow_the_density) {
  ge::render::PrecipSettings base;
  CHECK(ba::weather_particles(base, 1) == base.particles);
  CHECK(ba::weather_particles(base, 0.5) == base.particles / 2);
  CHECK(ba::weather_particles(base, 9) == base.particles * 2);
}
