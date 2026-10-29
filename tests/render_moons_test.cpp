// engine/render/moons.hpp and night.hpp's set_moons: ten kinds, each named once and seen at its
// real size from its planet; a set read from words and refused with a reason; the Earth's moon
// alone leaves the night as it was; each moon goes round at its own rate (Triton backwards) and
// is lit by the real sun; its light is its albedo times its solid angle's share of the sun's; a
// set without the Earth's moon lights the night by the others; a moon under a pixel is drawn a
// pixel and a half wide with its light shared over it; every sky type's set reads; every kind's
// map decodes at its size with its mean where the sky reads it, and its colour only colours.
//
// ge:summary Tests for other worlds' moons: kinds, sets, orbits, phases, light and the frame rows
// ge:keywords test render moons moon-kinds moon-set set_moons orbit retrograde phase other-moons
// ge:skill render testing
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "render/frame.hpp"
#include "render/moon_maps.hpp"
#include "render/moons.hpp"
#include "render/night.hpp"
#include "render/sky_style.hpp"

namespace {

namespace gr = ge::render;

constexpr double kDeg = 3.14159265358979323846 / 180;
constexpr double kJd = 2461100.5;  // a night in 2026

const gr::Moon kFullUp{40 * kDeg, 2.0, 1.0, gr::kMoonMeanKm};

gr::FrameView night_with(std::string_view words, double sun_deg = -20) {
  gr::FrameView v;
  v.sun_elevation = sun_deg * kDeg;
  v.sun_azimuth = 0.5;
  std::string problem;
  CHECK(gr::moon_set_named(words, v.moons, problem));
  gr::set_night(v, kFullUp, 46, 30);
  return v;
}

// The placed moon of the kind named `name`.
const gr::SkyMoon& placed(const gr::FrameView& v, const char* name) {
  for (int k = 0; k < v.night.moon_count; ++k) {
    if (v.night.moons[k].kind == gr::moon_kind_named(name)) return v.night.moons[k];
  }
  CHECK_MSG(false, std::string(name) + " is not placed");
  return v.night.moons[0];
}

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

}  // namespace

TEST(ten_kinds_each_named_once) {
  const auto kinds = gr::moon_kinds();
  CHECK(kinds.size() == 10);
  for (size_t i = 0; i < kinds.size(); ++i) {
    CHECK(gr::moon_kind_named(kinds[i].name) == static_cast<int>(i));
    CHECK(kinds[i].albedo > 0 && kinds[i].period_days > 0 && kinds[i].height_km > 0);
  }
  CHECK(gr::moon_kind_named("IO") == gr::moon_kind_named("io"));
  CHECK(gr::moon_kind_named("pluto") == -1);
}

TEST(each_kind_is_its_real_size_from_its_planet) {
  const auto kinds = gr::moon_kinds();
  const double earth = gr::moon_radius(gr::kMoonMeanKm);
  const auto r = [&](const char* name) {
    return gr::moon_kind_radius(kinds[static_cast<size_t>(gr::moon_kind_named(name))]);
  };
  // Io a little larger than the Moon, Phobos about a third, Deimos a sixteenth (2' across).
  CHECK(r("io") > earth && r("io") < 1.3 * earth);
  CHECK(r("phobos") > 0.3 * earth && r("phobos") < 0.5 * earth);
  CHECK(2 * r("deimos") < 3.0 / 60 * kDeg);
}

TEST(a_set_reads_from_words) {
  gr::MoonSet set;
  std::string problem;
  CHECK(gr::moon_set_named("moon io:2 phobos:1:3", set, problem) && problem.empty());
  CHECK(set.earth && set.count == 2);
  CHECK(set.moons[0].kind == gr::moon_kind_named("io") && set.moons[0].size == 2);
  CHECK(set.moons[1].spin_hours == 3 && set.moons[1].size == 1);
  CHECK(gr::moon_set_named("none", set, problem) && !set.earth && set.count == 0);
  CHECK(!gr::moon_set_named("io pluto", set, problem) &&
        problem.find("pluto") != std::string::npos);
  CHECK(!gr::moon_set_named("io:x", set, problem) && !problem.empty());
  CHECK(!gr::moon_set_named("io europa ganymede callisto titan", set, problem));
  // Every sky type's own set reads; the Earth's is its moon alone, the desert's has none of it.
  for (const gr::SkyType& t : gr::sky_types()) CHECK(gr::moon_set_named(t.moons, set, problem));
  CHECK(gr::moon_set_named(gr::sky_moons_named("earth"), set, problem) && set.earth &&
        set.count == 0);
  CHECK(gr::moon_set_named(gr::sky_moons_named("dust"), set, problem) && !set.earth &&
        set.count == 2);
}

TEST(the_earths_moon_alone_leaves_the_night_as_it_was) {
  gr::FrameView a;
  a.sun_elevation = -20 * kDeg;
  gr::set_night(a, kFullUp, 46, 30);
  gr::FrameView b = a;
  gr::set_moons(b, kJd);
  CHECK(b.night.moon_count == 0);
  const gr::Night& x = a.night;
  const gr::Night& y = b.night;
  CHECK(x.moon_key == y.moon_key && x.moon_illuminance == y.moon_illuminance);
  CHECK(x.moon_radius == y.moon_radius && x.moon_surface == y.moon_surface);
  CHECK(x.moon_elevation == y.moon_elevation && x.other_light == y.other_light);
  CHECK(x.earthshine == y.earthshine && x.moon_haze == y.moon_haze);
  CHECK(a.sun_elevation == b.sun_elevation && a.sun_illuminance == b.sun_illuminance);
}

TEST(each_goes_round_at_its_own_rate_triton_backwards) {
  // The sky held still (one sidereal angle): only the orbit moves a moon. Seen from the north
  // celestial pole a prograde moon turns one way, Triton the other.
  const auto turn = [](const char* name, double dt) {
    gr::FrameView v = night_with(std::string("moon ") + name);
    gr::set_moons(v, kJd);
    const gr::SkyMoon a = v.night.moons[0];
    gr::set_moons(v, kJd + dt);
    const gr::SkyMoon b = v.night.moons[0];
    double cross[3] = {a.toward[1] * b.toward[2] - a.toward[2] * b.toward[1],
                       a.toward[2] * b.toward[0] - a.toward[0] * b.toward[2],
                       a.toward[0] * b.toward[1] - a.toward[1] * b.toward[0]};
    return std::pair{dot3(cross, a.pole), std::acos(std::fmin(1.0, dot3(a.toward, b.toward)))};
  };
  const auto io = turn("io", 0.01);
  const auto triton = turn("triton", 0.01);
  CHECK(io.first * triton.first > 0);  // each the same way about its own pole
  gr::FrameView v = night_with("io triton");
  gr::set_moons(v, kJd);
  CHECK(dot3(placed(v, "io").pole, placed(v, "triton").pole) < -0.8);  // the poles opposed
  // The farther drawn first, so the nearer over it.
  CHECK(v.night.moons[0].kind == gr::moon_kind_named("io"));
  // Phobos's 7.65 hours go round 23 times as fast as Callisto's 16.7 days.
  const auto phobos = turn("phobos", 0.005);
  const auto callisto = turn("callisto", 0.005);
  CHECK(phobos.second > 40 * callisto.second);
}

TEST(a_locked_moon_keeps_its_face_toward_the_eye) {
  gr::FrameView v = night_with("europa ganymede:1:5");
  gr::set_moons(v, kJd);
  const gr::SkyMoon& europa = placed(v, "europa");
  // Its 0 N 0 E toward its planet, so away from where it is seen, within its libration.
  CHECK(dot3(europa.prime, europa.toward) < -0.999);
  // One spun by hand turns its face round.
  const double first = dot3(placed(v, "ganymede").prime, placed(v, "ganymede").toward);
  gr::set_moons(v, kJd + 1.25 / 24);
  const gr::SkyMoon& g = placed(v, "ganymede");
  CHECK(std::fabs(dot3(g.prime, g.toward) - first) > 0.1);
}

TEST(its_light_is_its_albedo_over_its_solid_angle) {
  gr::FrameView v = night_with("io");
  gr::set_moons(v, kJd);
  const gr::SkyMoon& m = placed(v, "io");
  const gr::MoonKind& io = gr::moon_kinds()[static_cast<size_t>(m.kind)];
  const double r = gr::moon_kind_radius(io);
  CHECK(std::fabs(m.lux / gr::phase_law(m.phase) / (gr::kSunLux * io.albedo * r * r) - 1) < 1e-9);
  // The same rule gives the Earth's moon its light: albedo 0.12 at its mean distance.
  const double moon = gr::moon_radius(gr::kMoonMeanKm);
  CHECK(std::fabs(gr::kSunLux * 0.12 * moon * moon / gr::kMoonLux - 1) < 0.05);
  // A full Io lights the snow several times a full moon.
  CHECK(gr::kSunLux * io.albedo * r * r > 5 * gr::kMoonLux);
  // The phase is the angle at the moon between the sun and the eye.
  double sun[3];
  gr::look_dir(v.night.sun_azimuth, v.night.sun_elevation, sun);
  CHECK(std::fabs(std::cos(m.phase) + dot3(sun, m.toward)) < 1e-9);
}

TEST(a_set_without_the_earths_moon_lights_the_night_by_the_others) {
  // Hour after hour until Io stands over the horizon at night.
  for (int h = 0; h < 48; ++h) {
    gr::FrameView v = night_with("io");
    gr::set_moons(v, kJd + h / 24.0);
    const gr::SkyMoon& io = placed(v, "io");
    CHECK(v.night.moon_radius == 0);  // the Earth's disc is gone
    if (io.elevation <= 0) {
      CHECK(!v.night.moon_key && v.sun_elevation == -20 * kDeg);
      continue;
    }
    CHECK(v.night.moon_key);
    CHECK(v.sun_elevation == io.elevation && v.sun_azimuth == io.azimuth);
    CHECK(std::fabs(v.night.moon_illuminance * gr::kSunLux - io.lux) < 1e-12);
    CHECK(v.night.sun_elevation == -20 * kDeg);
    return;
  }
  CHECK_MSG(false, "Io never rose in two days");
}

TEST(a_moon_under_a_pixel_is_drawn_a_pixel_and_a_half_wide) {
  // Io (the farther) first, Deimos over it.
  gr::FrameView v = night_with("deimos io");
  gr::set_moons(v, kJd);
  gr::FrameUniforms u;
  gr::fill_uniforms(u, v, 640, 360);
  const double pixel = v.fov_y / 360;
  const float* deimos = u.moons[gr::kMoonRows];
  const float* deimos_surface = u.moons[gr::kMoonRows + 2];
  CHECK(std::fabs(deimos[3] - 1.5 * pixel) < 1e-6);
  const double pre = gr::pre_exposure(v);
  const double shrink = v.night.moons[1].radius / (1.5 * pixel);
  CHECK(std::fabs(deimos_surface[3] / (v.night.moons[1].surface * pre * shrink * shrink) - 1) <
        1e-4);
  // Io, larger than a pixel, as it is; the moon's lens draws it larger.
  CHECK(std::fabs(u.moons[0][3] / v.night.moons[0].radius - 1) < 1e-6);
  v.moon_size = 3;
  gr::fill_uniforms(u, v, 640, 360);
  CHECK(std::fabs(u.moons[0][3] / (3 * v.night.moons[0].radius) - 1) < 1e-6);
  // No moon past the set's.
  CHECK(u.moons[static_cast<ptrdiff_t>(2) * gr::kMoonRows][3] == 0.0f);
}

TEST(every_kinds_map_decodes_at_its_mean) {
  const double pi = 3.14159265358979323846;
  for (int k = 0; k < static_cast<int>(gr::moon_kinds().size()); ++k) {
    const std::vector<uint8_t> map = gr::moon_kind_map(k);
    CHECK_MSG(map.size() == static_cast<size_t>(gr::kMoonMapsWidth) * gr::kMoonMapsHeight,
              std::string(gr::moon_kinds()[static_cast<size_t>(k)].name) +
                  "'s map does not decode at 1024 x 512: rebake engine/render/moon_maps/");
    // The mean over the sphere, each row weighted by the area it covers.
    double sum = 0, weight = 0;
    for (int y = 0; y < gr::kMoonMapsHeight; ++y) {
      const double w = std::cos(((y + 0.5) / gr::kMoonMapsHeight - 0.5) * pi);
      for (int x = 0; x < gr::kMoonMapsWidth; ++x) {
        sum += w * map[static_cast<size_t>(y) * gr::kMoonMapsWidth + x];
        weight += w;
      }
    }
    CHECK(std::fabs(sum / weight - gr::kMoonMapMean) < 2);
    // A colour map's luminance is 1 (128) everywhere, near enough: it only colours.
    const std::vector<uint8_t> colour = gr::moon_kind_colour(k);
    if (colour.empty()) continue;
    CHECK(colour.size() == static_cast<size_t>(gr::kMoonColourWidth) * gr::kMoonColourHeight * 3);
    for (size_t i = 0; i < colour.size(); i += 3) {
      const double y = 0.2126 * colour[i] + 0.7152 * colour[i + 1] + 0.0722 * colour[i + 2];
      CHECK(std::fabs(y - 128) < 3);
    }
  }
  // Io, Ganymede and Triton are in colour; the rest are drawn in their tints.
  CHECK(!gr::moon_kind_colour(gr::moon_kind_named("io")).empty());
  CHECK(gr::moon_kind_colour(gr::moon_kind_named("europa")).empty());
}
