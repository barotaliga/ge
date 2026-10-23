// engine/render/moon.hpp and the moon's face and shadow in engine/render/night.hpp: the albedo
// table decodes to its size, its near side's mean is 1 by its scale, the seas are darker than
// the highlands, the map wraps round the sphere and the table's codec round-trips; the shadow's
// light is whole outside the penumbra, falls across it to the air's red in the umbra and is
// continuous at both edges; an eclipse's mean light is the disc's; set_night lays the face and
// the shadow into the night and dims the moonlight by the eclipse.
//
// ge:summary Tests for the moon's face (albedo map) and the Earth's shadow on it
// ge:keywords test render moon albedo moon-map eclipse umbra penumbra shadow_light eclipse_of
// ge:keywords set_night moon-face
// ge:skill render testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "core/moon.hpp"
#include "core/solar.hpp"
#include "render/moon.hpp"
#include "render/night.hpp"

namespace {

namespace gr = ge::render;
namespace gc = ge::core;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

}  // namespace

TEST(the_albedo_map_decodes_and_its_near_side_means_one) {
  const std::vector<uint8_t>& m = gr::moon_albedo();
  CHECK(m.size() == static_cast<size_t>(gr::kMoonMapWidth) * gr::kMoonMapHeight);
  // The near side's mean as a full moon shows it, by the map's own sampler.
  double sum = 0, weight = 0;
  for (int y = 0; y < 90; ++y) {
    const double lat = -89 + y * 2.0;
    for (int x = 0; x < 90; ++x) {
      const double lon = -89 + x * 2.0;
      const double w = std::cos(lat * kDeg) * std::cos(lat * kDeg) * std::cos(lon * kDeg);
      sum += w * gr::albedo_at(lon, lat);
      weight += w;
    }
  }
  CHECK(std::fabs(sum / weight - 1) < 0.02);
}

TEST(the_seas_are_darker_than_the_highlands) {
  // The sea of tranquillity, the sea of crises, the ocean of storms against the southern
  // highlands and the far side's.
  const double seas[] = {gr::albedo_at(31.4, 8.5), gr::albedo_at(59.1, 17.0),
                         gr::albedo_at(-57.4, 18.4)};
  const double lands[] = {gr::albedo_at(10.0, -25.0), gr::albedo_at(180.0, 0.0)};
  for (double s : seas) {
    for (double l : lands) CHECK(l > 1.4 * s);
  }
}

TEST(the_map_wraps_round_the_sphere) {
  CHECK(std::fabs(gr::albedo_at(180.0, 10.0) - gr::albedo_at(-180.0, 10.0)) < 1e-12);
  CHECK(std::isfinite(gr::albedo_at(0.0, 90.0)) && std::isfinite(gr::albedo_at(0.0, -90.0)));
}

TEST(the_table_codec_round_trips) {
  std::vector<uint8_t> bytes;
  bytes.reserve(1000);
  for (int i = 0; i < 1000; ++i) bytes.push_back(static_cast<uint8_t>((i * 37 + 11) & 0xff));
  for (size_t n : {size_t{998}, size_t{999}, size_t{1000}}) {
    const std::vector<uint8_t> in(bytes.begin(), bytes.begin() + static_cast<long>(n));
    std::vector<uint8_t> out;
    CHECK(gr::decode_moon_map(gr::encode_moon_map(in), out));
    CHECK(out == in);
  }
}

TEST(the_bake_keeps_the_brightest_texel_and_scales_to_the_mean) {
  // A map of one grey and a brighter band: the brightest byte 255, the scale the near side's
  // mean back to 1.
  const int w = gr::kMoonMapWidth, h = gr::kMoonMapHeight;
  std::vector<double> lin(static_cast<size_t>(w) * h, 0.1);
  for (int x = 0; x < w; ++x) lin[static_cast<size_t>(h / 2) * w + x] = 0.2;
  std::vector<uint8_t> bytes;
  double scale = 0;
  CHECK(gr::bake_moon_map(lin, w, h, bytes, scale));
  CHECK(bytes[static_cast<size_t>(h / 2) * w] == 255);
  CHECK(bytes[0] == 128);
  CHECK(std::fabs(128 / 255.0 * scale - 1) < 0.01);
  CHECK(!gr::bake_moon_map(lin, w, h + 1, bytes, scale));
}

TEST(the_shadow_light_falls_across_the_penumbra_to_the_umbras_red) {
  const double u = 0.012, p = 0.0215;
  const gr::ShadowLight out = gr::shadow_light(p * 1.01, u, p);
  CHECK(out.sun == 1 && out.air == 0);
  const gr::ShadowLight edge = gr::shadow_light(p * 0.9999, u, p);
  CHECK(edge.sun > 0.999);
  double last = 2;
  for (int k = 0; k <= 20; ++k) {
    const double s = u + (p - u) * (1 - k / 20.0);
    const gr::ShadowLight l = gr::shadow_light(s, u, p);
    CHECK(l.sun + l.air <= last + 1e-12);
    last = l.sun + l.air;
  }
  const gr::ShadowLight inner = gr::shadow_light(u * 0.9999, u, p);
  CHECK(inner.sun < 1e-6);
  CHECK(std::fabs(inner.air - 1e-3) < 1e-5);
  const gr::ShadowLight axis = gr::shadow_light(0, u, p);
  CHECK(std::fabs(axis.air - 1e-4) < 1e-9);
  // No shadow at all.
  CHECK(gr::shadow_light(0, 0, 0).sun == 1);
}

TEST(an_eclipse_dims_the_moonlight_by_the_discs_mean) {
  // The 3 March 2026 eclipse in totality, then a day later.
  const double jd = gc::julian_day(2026, 3, 3) + 11.55 / 24;
  const gc::MoonSky total = gc::moon_sky(jd, 20.0, -155.0);
  const double t[3] = {total.toward.x, total.toward.y, total.toward.z};
  const double s[3] = {total.shadow.x, total.shadow.y, total.shadow.z};
  const gr::Eclipse e = gr::eclipse_of(t, total.angular_radius, s, total.umbra, total.penumbra);
  CHECK(e.mean < 1e-3 && e.mean > 1e-4);
  CHECK(e.peak >= e.mean && e.peak < 1.1e-3);
  const gc::MoonSky later = gc::moon_sky(jd + 1, 20.0, -155.0);
  const double t2[3] = {later.toward.x, later.toward.y, later.toward.z};
  const double s2[3] = {later.shadow.x, later.shadow.y, later.shadow.z};
  const gr::Eclipse none =
      gr::eclipse_of(t2, later.angular_radius, s2, later.umbra, later.penumbra);
  CHECK(none.mean == 1 && none.peak == 1);

  gr::FrameView v;
  v.sun_elevation = -30 * kDeg;
  v.sun_azimuth = 0;
  gr::set_night(v, gr::moon_of(total), 20.0, 0.0);
  const double dimmed = v.night.moon_key ? v.sun_illuminance : v.night.moon_illuminance;
  gr::FrameView w;
  w.sun_elevation = -30 * kDeg;
  w.sun_azimuth = 0;
  gr::Moon clear = gr::moon_of(total);
  clear.umbra = clear.penumbra = 0;
  gr::set_night(w, clear, 20.0, 0.0);
  const double full = w.night.moon_key ? w.sun_illuminance : w.night.moon_illuminance;
  CHECK(dimmed < full * 1e-3);
  // The night places the disc where it is seen, refraction lifting it a few arcminutes into the
  // shadow's light, so its peak is near the airless one, not equal to it.
  CHECK(v.night.eclipse_peak > e.mean && v.night.eclipse_peak < 1.5 * e.peak);
  CHECK(v.night.penumbra == total.penumbra);
}

TEST(set_night_lays_the_face_in_and_a_bare_moon_gets_one) {
  const gc::MoonSky m = gc::moon_sky(gc::julian_day(2026, 1, 3), 46.0, 7.0);
  gr::FrameView v;
  v.sun_elevation = -20 * kDeg;
  gr::set_night(v, gr::moon_of(m), 46.0, 100.0);
  CHECK(v.night.moon_pole[0] == m.pole.x && v.night.moon_prime[2] == m.prime.z);
  CHECK(v.night.albedo_scale == gr::moon_albedo_scale());
  // A moon with no face: the near side square to the eye, the pole up the sky, east as the
  // real face's handedness has it (prime x pole).
  gr::FrameView b;
  b.sun_elevation = -20 * kDeg;
  gr::set_night(b, {40 * kDeg, 3.0, 0.5, gr::kMoonMeanKm}, 46.0, 100.0);
  const double* p = b.night.moon_pole;
  const double* q = b.night.moon_prime;
  const double* e = b.night.moon_east;
  CHECK(std::fabs(p[0] * q[0] + p[1] * q[1] + p[2] * q[2]) < 1e-9);
  CHECK(std::fabs(p[0] * e[0] + p[1] * e[1] + p[2] * e[2]) < 1e-9);
  // The real face's handedness: east is prime x pole in the world's axes.
  const double cx = m.prime.y * m.pole.z - m.prime.z * m.pole.y;
  CHECK(std::fabs(cx - m.east.x) < 1e-9);
  const double bx = q[1] * p[2] - q[2] * p[1];
  CHECK(std::fabs(bx - e[0]) < 1e-9);
  CHECK(p[1] > 0);
}
