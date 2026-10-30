// Our sky picked by a classic map's sky texture (bsp_sky.hpp).
//
// ge:summary Our sky picked by a classic map's sky texture: the rules from hue and brightness
// ge:keywords bsp brush sky sky-type match pick_sky hue dusk tint night_over clouds_of dress_sky
// ge:skill brush render
// ge:test tests/brush_bsp_sky_test.cpp
#include "brush/app/bsp_sky.hpp"

#include <algorithm>
#include <cmath>

#include "core/num_text.hpp"
#include "render/cloud_kinds.hpp"
#include "render/clouds.hpp"
#include "render/night.hpp"
#include "wind/aloft.hpp"

namespace ge::brush::app {

namespace {

double luminance(const double c[3]) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }

std::string pct(double v) { return core::num::to_fixed(v * 100, 0) + "%"; }

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The latitude the classic night's stars turn over, degrees.
constexpr double kLatitude = 46;
// The light of a torch-lit hall's white wall, cd/m^2: the eye stays adapted to it at night (over
// the exposure's own adaptation floor, 14 lx on white), so the rooms and the lava keep the day's
// look and the sky through the sky faces reads as the dark it is.
constexpr double kRoomCd = 5;
// The night sky as seen through the sky faces lifted toward this share of that wall's light (the
// moonlit cloud's white against the halls'), as a photograph of a lit hall under the night shows
// its window; never lifted past kMostSeen, so a sky type's own glow (a nebula's) stays a glow.
constexpr double kSkyShare = 0.15;
constexpr double kMostSeen = 40;

// A map's name to a seed (FNV-1a), and a seed and a salt to 0..1.
uint32_t seed_of(std::string_view name) {
  uint32_t h = 2166136261u;
  for (const char c : name) {
    h ^= static_cast<uint8_t>(c);
    h *= 16777619u;
  }
  return h;
}

double hash01(uint32_t seed, uint32_t salt) {
  uint32_t h = seed ^ (salt * 0x27d4eb2du);
  h *= 0x9e3779b1u;
  h ^= h >> 16;
  h *= 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;
  return static_cast<double>(h) / 4294967296.0;
}

double deal(const double band[2], double u) { return band[0] + (band[1] - band[0]) * u; }

// A night layer of `kind` over the map's top, dealt off the pick's seed on `salt`, its cover
// `cover`, carried `t` s on the wind aloft at its height.
void night_layer(const render::CloudKind& kind, uint32_t salt, double cover, const SkyPick& p,
                 double top_y, double t, render::CloudLayer& l) {
  auto u = [&](uint32_t k) { return hash01(p.seed, 101 + salt * 17 + k); };
  const double base = deal(kind.base, u(0));
  const double depth = deal(kind.depth, u(1));
  const double h = base + 0.5 * depth;
  l.base_m = top_y + base;
  l.top_m = l.base_m + depth;
  render::take_form(kind, depth, l);
  l.cover = cover;
  l.cell_m = deal(kind.cell, u(3));
  l.heading = p.wind_from + kPi + ge::wind::veer_aloft(h, kLatitude);
  const double tile = l.cell_m * l.stretch * render::kCloudTileCells;
  const double dusk = ge::wind::dusk_of(p.sun_elevation * kDeg);
  l.drift_m =
      u(4) * tile + ge::wind::wind_aloft(p.wind, h, kind.speed) * ge::wind::jet_at(h, dusk) * t;
  l.evolve = u(5) * render::kCloudTileCells + t / kind.life;
}

}  // namespace

SkyPick pick_sky(const SkyColours& c) {
  SkyPick p;
  if (!c.found) {
    render::sky_style_named(p.type, p.style);
    p.why = "no sky texture: the Earth's fair day";
    return p;
  }
  // The sky as seen: the back layer under the cloud layer's share.
  double seen[3];
  for (int k = 0; k < 3; ++k) seen[k] = c.back[k] * (1 - c.cover) + c.cloud[k] * c.cover;
  const double y = luminance(seen);
  const double hi = std::max({seen[0], seen[1], seen[2]});
  const double lo = std::min({seen[0], seen[1], seen[2]});
  const double sat = hi > 0 ? (hi - lo) / hi : 0;
  const double r = seen[0] / std::max(hi, 1e-9), g = seen[1] / std::max(hi, 1e-9),
               b = seen[2] / std::max(hi, 1e-9);
  std::string hue;
  if (sat < 0.2) {
    p.type = y < 0.01 ? "void" : "ash";
    hue = y < 0.01 ? "black" : "grey";
  } else if (r >= g && r >= b) {
    if (g > 0.65) {
      p.type = "dust";
      hue = "amber";
    } else if (y < 0.03) {
      p.type = "ash";  // a dark red-brown smoke
      hue = "dark red-brown";
    } else {
      p.type = "hell";
      hue = "red";
    }
  } else if (g >= r && g >= b) {
    p.type = "toxic";
    hue = "green";
  } else if (r >= 0.95 * g && r > 0.35) {
    p.type = "nebula";
    hue = "violet";
  } else {
    p.type = y > 0.3 ? "ice" : "earth";
    hue = "blue";
  }
  render::sky_style_named(p.type, p.style);
  if (p.type == "ash") {
    // The ash's air thinned: its smoke drinks so much of the day that the exposure, keyed on the
    // day's light, would lift the lava and the lamps; the veil darkens the sky instead.
    render::SkyKnobs thin;
    thin.air = 0.35;
    render::with_knobs(p.style, thin);
  }
  // The sky's light against the rooms by the sky's brightness, never so dim that the sky reads as
  // a hole. The sun stays well up: the exposure keys the day's light on a white face, so a low sun
  // would brighten the lava and the lamps against the rooms; the veil darkens the sky instead.
  p.sun_elevation = std::clamp(25 + 60 * y, 25.0, 45.0);
  p.sky_light = std::clamp(std::sqrt(y / 0.25), 0.2, 1.0);
  // A tint halfway toward the texture's own hue, its brightest channel 1.
  for (int k = 0; k < 3; ++k) p.tint[k] = std::sqrt(seen[k] / std::max(hi, 1e-9));
  p.cover = std::clamp(c.cover, 0.1, 0.9);
  p.why = "a " + hue + " sky (" + pct(y) + " bright, " + pct(sat) + " saturated) with " +
          pct(c.cover) + " cloud: " + p.type + ", the sun at " +
          core::num::to_fixed(p.sun_elevation, 0) + " degrees, the sky at " + pct(p.sky_light) +
          " against the rooms";
  return p;
}

bool with_type(SkyPick& pick, std::string_view name) {
  render::SkyStyle style;
  if (!render::sky_style_named(name, style)) return false;
  pick.type = std::string(name);
  pick.style = style;
  return true;
}

void night_over(SkyPick& p, std::string_view map) {
  p.night = true;
  p.seed = seed_of(map);
  auto u = [&](uint32_t k) { return hash01(p.seed, k); };
  const bool evening = u(0) < 0.4;
  p.sun_azimuth = 360 * u(2);
  if (evening) {
    // The twilight's glow on one side, the full moon rising on the other.
    p.sun_elevation = -3 - 3 * u(1);
    p.moon_azimuth = p.sun_azimuth + 180 + 30 * (u(3) - 0.5);
    p.moon_elevation = 8 + 14 * u(4);
    p.moon_lit = 0.93 + 0.07 * u(5);
  } else {
    p.sun_elevation = -20 - 15 * u(1);
    p.moon_azimuth = 360 * u(3);
    p.moon_elevation = 25 + 25 * u(4);
    p.moon_lit = 0.55 + 0.45 * u(5);
  }
  p.sidereal = 360 * u(6);
  p.wind = 8 + 6 * u(7);
  p.wind_from = 2 * kPi * u(8);
  p.scud_cover = 0.3 + 0.15 * u(9);
  p.deck_cover = 0.4 + 0.15 * u(10);
  p.sky_light = 1;
  p.adapt_cd = kRoomCd;
  // The light the night sky gives a level face: the twilight's, the moon's through the air and
  // the dark sky's own.
  const double moon = render::moonlight_on_ground(
      render::moon_lux(render::phase_angle(p.moon_lit), render::kMoonMeanKm),
      p.moon_elevation * kDeg);
  const double lux = render::twilight_lux(p.sun_elevation * kDeg) + moon + render::kDarkSkyLux;
  p.sky_seen = std::clamp(kSkyShare * kRoomCd * kPi / lux, 1.0, kMostSeen);
  p.why += "; " + std::string(evening ? "an evening" : "a night") + ", the sun " +
           core::num::to_fixed(-p.sun_elevation, 0) + " degrees down, the moon " +
           core::num::to_fixed(p.moon_elevation, 0) + " up at a heading of " +
           core::num::to_fixed(std::fmod(p.moon_azimuth, 360.0), 0) + " and " + pct(p.moon_lit) +
           " lit, a wind of " + core::num::to_fixed(p.wind, 0) + " m/s, scud over " +
           pct(p.scud_cover) + " and a broken deck over " + pct(p.deck_cover) +
           " of the sky, seen " + core::num::to_fixed(p.sky_seen, 0) + " times brighter";
}

render::Clouds clouds_of(const SkyPick& p, double top_y, double t) {
  render::Clouds c;
  if (!p.night) {
    if (p.cover <= 0) return c;
    c.count = 1;
    render::CloudLayer& l = c.layers[0];
    l.base_m = 1500;
    l.top_m = 2300;
    l.cover = p.cover;
    l.cell_m = p.cell_m;
    l.stretch = 1.3;
    l.dome = 1;
    l.density = 28.0 / 800;
    l.shadow_tau = 2.4;
    return c;
  }
  c.count = 2;
  night_layer(*render::cloud_kind_named("fractus"), 0, p.scud_cover, p, top_y, t, c.layers[0]);
  night_layer(*render::cloud_kind_named("stratocumulus"), 1, p.deck_cover, p, top_y, t,
              c.layers[1]);
  return c;
}

void dress_sky(render::FrameView& v, const SkyPick& p, double top_y, double t) {
  v.style = p.style;
  v.sun_elevation = p.sun_elevation * kDeg;
  v.sun_azimuth = p.sun_azimuth * kDeg;
  v.time_s = t;
  v.clouds = clouds_of(p, top_y, t);
  if (!p.night) return;
  const render::Moon moon{p.moon_elevation * kDeg, p.moon_azimuth * kDeg, p.moon_lit,
                          render::kMoonMeanKm};
  render::set_night(v, moon, kLatitude, p.sidereal);
  v.adapt_cd = p.adapt_cd;
}

}  // namespace ge::brush::app
