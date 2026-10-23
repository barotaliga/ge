// engine/render/night.hpp: the phase law is 1 at full and a half moon gives about a tenth of a
// full one's light; the phase angle runs from 0 at full to pi at new; the surge is 1 at full and
// grows toward a crescent; the twilight's light falls with the sun; a day leaves the night unset
// and the pre-exposure at 1; at night the moon keys once the sun is three degrees under and it
// is up, and the frame's uniforms carry the night's numbers through the pre-exposure.
//
// ge:summary Tests for the night: phase law, surge, twilight, the key light, the pre-exposure
// ge:keywords test render night moon phase_law surface_surge twilight_lux set_night pre_exposure
// ge:keywords key-light
// ge:skill render testing
#include <cmath>

#include "check.hpp"
#include "render/frame.hpp"
#include "render/night.hpp"

namespace {

namespace gr = ge::render;

constexpr double kDeg = 3.14159265358979323846 / 180;

gr::FrameView sun_at(double elevation_deg) {
  gr::FrameView v;
  v.sun_elevation = elevation_deg * kDeg;
  v.sun_azimuth = 1.0;
  return v;
}

const gr::Moon kFullUp{40 * kDeg, 2.0, 1.0, gr::kMoonMeanKm};

}  // namespace

TEST(phase_law_is_one_at_full_and_a_tenth_at_half) {
  CHECK(gr::phase_law(0) == 1);
  const double half = gr::phase_law(90 * kDeg);
  CHECK(half > 0.07 && half < 0.11);
  CHECK(gr::phase_law(30 * kDeg) > gr::phase_law(60 * kDeg));
}

TEST(phase_angle_runs_from_full_to_new) {
  CHECK(std::fabs(gr::phase_angle(1)) < 1e-12);
  CHECK(std::fabs(gr::phase_angle(0.5) - 90 * kDeg) < 1e-12);
  CHECK(std::fabs(gr::phase_angle(0) - 180 * kDeg) < 1e-12);
}

TEST(moon_lux_and_radius_at_the_mean_distance) {
  CHECK(std::fabs(gr::moon_lux(0, gr::kMoonMeanKm) - gr::kMoonLux) < 1e-12);
  // Nearer is brighter by the square of the distance.
  CHECK(std::fabs(gr::moon_lux(0, gr::kMoonMeanKm / 2) - 4 * gr::kMoonLux) < 1e-12);
  // About 31' across.
  const double across = 2 * gr::moon_radius(gr::kMoonMeanKm) / kDeg * 60;
  CHECK(across > 30.5 && across < 31.5);
}

TEST(surge_is_one_at_full_and_grows_toward_a_crescent) {
  CHECK(std::fabs(gr::surface_surge(0) - 1) < 1e-12);
  CHECK(gr::surface_surge(60 * kDeg) < 1);
  CHECK(gr::surface_surge(150 * kDeg) > 0);
}

TEST(twilight_falls_with_the_sun) {
  CHECK(gr::twilight_lux(0.1) >= 1e4);
  double last = gr::twilight_lux(0);
  for (int d = 1; d <= 20; ++d) {
    const double lux = gr::twilight_lux(-d * kDeg);
    CHECK(lux < last);
    last = lux;
  }
  // Civil twilight's end (-6 degrees) lights snow a few lux.
  const double civil = gr::twilight_lux(-6 * kDeg);
  CHECK(civil > 1 && civil < 20);
}

TEST(a_day_leaves_the_night_unset) {
  gr::FrameView v = sun_at(20);
  gr::set_night(v, kFullUp, 46, 30);
  CHECK(v.night.moon_illuminance == 0 && v.night.dark_sky == 0 && !v.night.moon_key);
  CHECK(v.sun_elevation == 20 * kDeg);
  CHECK(v.night.moon_radius > 0 && v.night.moon_surface > 0);
  CHECK(gr::pre_exposure(v) == 1);
  gr::FrameUniforms u;
  gr::fill_uniforms(u, v, 64, 36);
  CHECK(u.world[1] == 1.0f && u.night[0] == 0.0f);
}

TEST(the_moon_keys_once_the_sun_is_gone) {
  // Two degrees under, the sun still keys; the moon's light is spread from above.
  gr::FrameView dusk = sun_at(-2);
  gr::set_night(dusk, kFullUp, 46, 30);
  CHECK(!dusk.night.moon_key && dusk.night.other_light > 0);
  CHECK(dusk.sun_elevation == -2 * kDeg);
  // Four under, the moon does: the view's key light is the moon's.
  gr::FrameView night = sun_at(-4);
  gr::set_night(night, kFullUp, 46, 30);
  CHECK(night.night.moon_key && night.night.other_light == 0);
  CHECK(night.sun_elevation == kFullUp.elevation && night.sun_azimuth == kFullUp.azimuth);
  CHECK(night.night.sun_elevation == -4 * kDeg);
  CHECK(std::fabs(night.sun_illuminance * gr::kSunLux - gr::kMoonLux) < 1e-9);
  // A moon under the horizon keys nothing.
  gr::FrameView set = sun_at(-20);
  gr::set_night(set, {-5 * kDeg, 2.0, 1.0, gr::kMoonMeanKm}, 46, 30);
  CHECK(!set.night.moon_key && set.sun_elevation == -20 * kDeg);
}

TEST(the_pre_exposure_keeps_the_night_in_range) {
  gr::FrameView v = sun_at(-20);
  gr::set_night(v, kFullUp, 46, 30);
  const double pre = gr::pre_exposure(v);
  CHECK(pre > 1e3);
  gr::FrameUniforms u;
  gr::fill_uniforms(u, v, 64, 36);
  CHECK(std::fabs(u.world[1] / pre - 1) < 1e-6);
  // The key light's illuminance is stored through it, near a unit.
  CHECK(std::fabs(u.sun[3] / (v.sun_illuminance * pre) - 1) < 1e-6);
  CHECK(u.sun[3] > 0.01f && u.sun[3] < 10.0f);
  CHECK(u.moon_light[3] == 1.0f);
  CHECK(std::fabs(u.night[2] - 30 * kDeg) < 1e-6 && std::fabs(u.night[3] - 46 * kDeg) < 1e-6);
}
