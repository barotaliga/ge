// The snow's air (engine/snow/air.hpp): the layer's air is the climate's at its place (the lapse
// to its height included) and two days meet round midnight with no jump; new snow is lighter the
// colder the air it fell through, weighted by the fall; a warm day then a frost leaves a crust the
// layer reads and the feel names; a cold, clear night's snow squeaks and glides worse than a
// mild day's; a fall on a cold day lies deeper than the same water on a mild one.
//
// ge:summary Tests: the snow's air (the climate's air, fresh density, crust, feel, squeak, glide)
// ge:keywords test snow air weather climate fresh_density crust feel squeak glide
// ge:skill snow testing
#include <cmath>

#include "check.hpp"
#include "snow/air.hpp"
#include "snow/layer.hpp"
#include "snow/time.hpp"

namespace sn = ge::snow;
namespace gw = ge::wind;

namespace {

constexpr double kHour = 3600;

gw::Climate day(double mean, double range = 10, double cloud = 0.2, double wind = 2) {
  gw::Climate c;
  c.mean_c = mean;
  c.range_c = range;
  c.cloud = cloud;
  c.wind = wind;
  return c;
}

sn::LayerSpec flat(double depth, const sn::Weather* w) {
  sn::LayerSpec s;
  s.region = {-4, -4, 4, 4};
  s.cover.depth = depth;
  s.weather = w;
  return s;
}

// Six hours of 2 mm/h of water from `t0`.
sn::Schedule storm(double t0) {
  return {{t0, t0 + 1, t0 + 6 * kHour, t0 + 6 * kHour + 1}, {0, 2.0 / 3600, 2.0 / 3600, 0}};
}

}  // namespace

TEST(the_layers_air_is_the_climates_at_its_place) {
  sn::SnowAir a;
  a.days = {day(-4)};
  a.site.y = 500;
  for (int i = 0; i < 16; i++) {
    const double h = 1.5 * i;
    CHECK(std::fabs(sn::air_c(a, h * kHour) - gw::air_at(a.days[0], a.site, h).temp_c) < 1e-12);
  }
  // Higher is colder.
  sn::SnowAir high = a;
  high.site.y = 1500;
  CHECK(sn::air_c(high, 14 * kHour) < sn::air_c(a, 14 * kHour) - 6);
  // The schedule holds the same air.
  const sn::Schedule s = sn::air_schedule(a, 48 * kHour);
  CHECK(s.t.front() == 0 && s.t.back() == 48 * kHour);
  CHECK(std::fabs(sn::value_at(s, 9 * kHour) - sn::air_c(a, 9 * kHour)) < 1e-12);
  // No days: 0.
  CHECK(sn::air_c(sn::SnowAir{}, 3600) == 0);
}

TEST(two_days_meet_round_midnight_without_a_jump) {
  sn::SnowAir a;
  a.days = {day(4), day(-8)};
  double prev = sn::air_c(a, 20 * kHour);
  for (int i = 0; i <= 160; i++) {
    const double h = 20 + 0.05 * i;
    const double v = sn::air_c(a, h * kHour);
    CHECK(std::fabs(v - prev) < 0.3);
    prev = v;
  }
  // Each day's own away from midnight; the last held after.
  CHECK(std::fabs(sn::air_c(a, 14 * kHour) - gw::air_at(a.days[0], a.site, 14).temp_c) < 1e-12);
  CHECK(std::fabs(sn::air_c(a, 38 * kHour) - gw::air_at(a.days[1], a.site, 14).temp_c) < 1e-12);
  CHECK(std::fabs(sn::air_c(a, 86 * kHour) - gw::air_at(a.days[1], a.site, 14).temp_c) < 1e-12);
  // The start hour shifts the layer's clock.
  sn::SnowAir late = a;
  late.start_hour = 8;
  CHECK(std::fabs(sn::air_c(late, 6 * kHour) - sn::air_c(a, 14 * kHour)) < 1e-12);
  // Each day's wind.
  const sn::Schedule w = sn::wind_schedule(a, 48 * kHour);
  CHECK(sn::value_at(w, 10 * kHour) == 2);
  a.days[1].wind = 9;
  CHECK(sn::value_at(sn::wind_schedule(a, 48 * kHour), 30 * kHour) == 9);
  CHECK(sn::value_at(sn::wind_schedule(a, 48 * kHour), 10 * kHour) == 2);
}

TEST(new_snow_is_lighter_the_colder_the_air_it_fell_through) {
  const sn::Schedule air_cold{{0}, {-12}}, air_mild{{0}, {-1}};
  const sn::Schedule fall = storm(0);
  const double cold = sn::fresh_density_over(fall, air_cold, 0, 12 * kHour);
  const double mild = sn::fresh_density_over(fall, air_mild, 0, 12 * kHour);
  CHECK(std::fabs(cold - gw::fresh_density(-12)) < 1e-9);
  CHECK(std::fabs(mild - gw::fresh_density(-1)) < 1e-9);
  CHECK(cold < 75 && mild > 95);
  // Weighted by the fall: snow only in the cold hours is the cold's.
  const sn::Schedule swing{{0, 6 * kHour, 7 * kHour}, {-12, -12, -1}};
  CHECK(std::fabs(sn::fresh_density_over(fall, swing, 0, 24 * kHour) - cold) < 1e-6);
  // Nothing falling: the air's mean.
  const double none = sn::fresh_density_over(sn::Schedule{}, air_mild, 0, kHour);
  CHECK(std::fabs(none - mild) < 1e-9);
}

TEST(a_cold_days_fall_lies_deeper_than_a_mild_days) {
  sn::SnowAir cold, mild;
  cold.days = {day(-12, 6, 0.9, 1)};
  mild.days = {day(-1, 2, 0.9, 1)};
  const sn::Weather wc = sn::weather_of(cold, storm(0), 24 * kHour);
  const sn::Weather wm = sn::weather_of(mild, storm(0), 24 * kHour);
  CHECK(wc.fresh_density < wm.fresh_density - 20);
  sn::SnowWorld world;
  sn::Layer& lc = sn::add_layer(world, flat(0.2, &wc));
  sn::Layer& lm = sn::add_layer(world, flat(0.2, &wm));
  const double dc = sn::sample(lc, 0, 0, 6 * kHour + 2).depth;
  const double dm = sn::sample(lm, 0, 0, 6 * kHour + 2).depth;
  CHECK(dc > dm + 0.03);
  // Fresh powder on both.
  CHECK(sn::feel_at(cold, wc, 7 * kHour).kind == gw::Feel::powder);
  CHECK(sn::feel_at(cold, wc, 7 * kHour).fresh > 0.15);
}

TEST(a_thaw_then_a_frost_leaves_a_crust) {
  sn::SnowAir a;
  a.days = {day(5, 10, 0.1, 1), day(-8, 10, 0.1, 1)};
  const sn::Weather w = sn::weather_of(a, sn::Schedule{}, 48 * kHour);
  sn::SnowWorld world;
  sn::Layer& l = sn::add_layer(world, flat(0.6, &w));
  // The warm afternoon: wet spring snow, no lid; slushy underfoot and slow.
  CHECK(sn::sample(l, 0, 0, 14 * kHour).kind == sn::Kind::spring);
  CHECK(sn::crust_load(w, 14 * kHour) == 0);
  const sn::SnowFeel noon = sn::feel_at(a, w, 14 * kHour);
  CHECK(noon.kind == gw::Feel::slush && noon.wet > 0);
  // The next morning: a crust that bears a boot.
  CHECK(sn::sample(l, 0, 0, 32 * kHour).kind == sn::Kind::crust);
  CHECK(sn::crust_load(w, 32 * kHour) > 785);
  const sn::SnowFeel dawn = sn::feel_at(a, w, 32 * kHour);
  CHECK(dawn.thawed && dawn.kind == gw::Feel::crust);
  CHECK(dawn.glide < noon.glide);
}

TEST(a_cold_clear_night_squeaks_and_glides_worse) {
  sn::SnowAir a;
  a.days = {day(-12, 8, 0, 1)};
  const sn::Weather w = sn::weather_of(a, sn::Schedule{}, 24 * kHour);
  const sn::SnowFeel night = sn::feel_at(a, w, 4 * kHour);
  CHECK(night.squeak && night.kind == gw::Feel::squeaky);
  CHECK(night.snow_c <= night.air_c);
  sn::SnowAir mild;
  mild.days = {day(-3, 2, 0.9, 1)};
  const sn::Weather wm = sn::weather_of(mild, sn::Schedule{}, 24 * kHour);
  const sn::SnowFeel soft = sn::feel_at(mild, wm, 14 * kHour);
  CHECK(!soft.squeak && soft.glide < night.glide);
  CHECK(soft.snow_c <= 0);
}
