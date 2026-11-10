// The title's sky and frame (title.hpp).
//
// ge:summary The title's night: the eclipsed moon's place and shadow, the cloud, the eye on it
// ge:keywords title title_sky title_view make_title_world blood-moon eclipse umbra clouds
// ge:skill render shooter
#include "shooters_edge/client/title.hpp"

#include <cmath>

#include "render/night.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

namespace ba = ge::brush::app;
namespace gr = ge::render;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The moon's place: low, so the cloud passes in front of it, and to the eye's front; the eye
// looks left of it and a little under it, so it stands right of the menu's column, the horizon
// low in the frame.
constexpr double kMoonElevation = 24, kMoonAzimuth = 200;  // degrees
constexpr double kLookLeft = 17, kLookUnder = 7;           // degrees
// The cloud played this many times faster than life: a deck a few km up drifts across the disc
// in about half a minute, slow enough to sit behind a menu (12 raced across it in a few seconds).
constexpr double kPace = 2.5;
// The Earth's shadow at the moon's distance, rad (a central eclipse's umbra and penumbra), and
// its axis set off the disc's middle toward its top (a fraction of the moon's radius): the lower
// limb, nearer the umbra's edge, a brighter red.
constexpr double kUmbra = 0.0125, kPenumbra = 0.0225, kAxisOff = 0.6;
// The moon drawn this many times nearer than it is, so its disc fills a third of the frame's
// height (about 17 degrees across in the 50 degree view), the title's subject: its surface as
// bright as ever (its light grows with its disc), the shadow at its distance grown with it.
constexpr double kNearer = 32;
// The light the eye is adapted to, cd/m^2: little more than the eclipsed moon gives, so the sky
// stays a night's, the cloud reads as dark shapes against it and the moon's red carries the
// picture.
constexpr double kAdaptCd = 0.01;

}  // namespace

ba::SkyPick title_sky() {
  ba::SkyPick p;
  // The red-tinted air (sky_style's `hell`): the night's glow and the cloud's own light take the
  // eclipse's red, where the Earth's air would light them a moonlit white.
  ba::with_type(p, "hell");
  ba::night_over(p, "title");
  p.sun_elevation = -30;
  p.sun_azimuth = kMoonAzimuth + 180;  // the moon full, opposite the sun
  p.moon_elevation = kMoonElevation;
  p.moon_azimuth = kMoonAzimuth;
  p.moon_lit = 1;
  p.wind = 22;
  p.wind_from = (kMoonAzimuth - 70) * kDeg;  // across the line of sight, not along it
  p.layer_count = 0;
  ba::add_layer(p, "fractus", 0.3);
  ba::add_layer(p, "stratocumulus", 0.38);
  p.adapt_cd = kAdaptCd;
  return p;
}

gr::FrameView title_view(double t_s) {
  const ba::SkyPick p = title_sky();
  gr::FrameView v;
  v.eye[0] = 0;
  v.eye[1] = 2;
  v.eye[2] = 0;
  // A slow sway of a degree, so the frame is never still even where no cloud passes.
  v.heading = (kMoonAzimuth - kLookLeft + 1.0 * std::sin(t_s * 0.07)) * kDeg;
  v.pitch = (kMoonElevation - kLookUnder + 0.4 * std::sin(t_s * 0.05 + 1)) * kDeg;
  v.fov_y = 50 * kDeg;
  v.near_m = 0.1;
  // The night as dress_sky would set it (the type, the sun under the horizon, the cloud, here
  // carried kPace times faster), the moon set once, in the Earth's shadow: its axis near the
  // moon's own direction, a little toward its top.
  v.style = p.style;
  v.sun_elevation = p.sun_elevation * kDeg;
  v.sun_azimuth = p.sun_azimuth * kDeg;
  v.clouds = ba::clouds_of(p, 0, t_s * kPace);
  v.time_s = t_s;
  double up[3];
  const double km = gr::kMoonMeanKm / kNearer;
  const double off = kAxisOff * gr::moon_radius(km);
  gr::look_dir(p.moon_azimuth * kDeg, p.moon_elevation * kDeg + off, up);
  gr::Moon moon{p.moon_elevation * kDeg, p.moon_azimuth * kDeg, 1, km};
  for (int k = 0; k < 3; ++k) moon.shadow[k] = up[k];
  moon.umbra = kUmbra * kNearer;
  moon.penumbra = kPenumbra * kNearer;
  gr::set_night(v, moon, 46, p.sidereal);
  v.adapt_cd = p.adapt_cd;
  return v;
}

WorldDrawPtr make_title_world(ge::gpu::Device& device, uint32_t width, uint32_t height,
                              wgpu::TextureFormat format, const look::Picture& picture,
                              std::string& error) {
  const SearchPath none;
  LoadedMap sky_only;
  sky_only.name = "title";
  WorldDrawPtr w =
      create_world_draw(device, none, sky_only, width, height, format, picture, {}, error);
  if (w) own_sky(*w, true);
  return w;
}

}  // namespace shooters_edge::client
