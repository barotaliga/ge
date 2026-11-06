// Our sky picked by a classic map's sky texture (bsp_sky.hpp).
//
// ge:summary Our sky picked by a classic map's sky texture: the rules from hue and brightness
// ge:keywords bsp brush sky sky-type match pick_sky hue dusk tint night_over clouds_of dress_sky
// ge:skill brush render
// ge:test tests/brush_bsp_sky_test.cpp
#include "brush/app/bsp_sky.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/num_text.hpp"
#include "core/solar.hpp"
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

// Layer `i` of the pick's stack over the map's top, dealt off the pick's seed on `i`, carried `t`
// s on the wind aloft at its height (and the evening's jet).
void deal_layer(const SkyLayer& sl, uint32_t i, const SkyPick& p, double top_y, double t,
                render::CloudLayer& l) {
  const render::CloudKind& kind = *sl.kind;
  auto u = [&](uint32_t k) { return hash01(p.seed, 101 + i * 17 + k); };
  const double base = deal(kind.base, u(0));
  const double depth = deal(kind.depth, u(1));
  const double h = base + 0.5 * depth;
  l.base_m = top_y + base;
  l.top_m = l.base_m + depth;
  render::take_form(kind, depth, l);
  l.cover = sl.cover;
  l.cell_m = deal(kind.cell, u(3)) * sl.cell;
  l.patch = std::max(l.patch, sl.patch);
  l.warp = sl.warp;
  l.heading = p.wind_from + kPi + ge::wind::veer_aloft(h, kLatitude);
  const double tile = l.cell_m * l.stretch * render::kCloudTileCells;
  const double dusk = ge::wind::dusk_of(p.sun_elevation * kDeg);
  l.drift_m =
      u(4) * tile + ge::wind::wind_aloft(p.wind, h, kind.speed) * ge::wind::jet_at(h, dusk) * t;
  l.evolve = u(5) * render::kCloudTileCells + t / kind.life;
}

}  // namespace

void add_layer(SkyPick& p, const char* name, double cover, double patch, double warp, double cell) {
  if (p.layer_count >= kSkyLayers) return;
  SkyLayer& l = p.layers[p.layer_count++];
  l.kind = render::cloud_kind_named(name);
  l.cover = cover;
  l.patch = patch;
  l.warp = warp;
  l.cell = cell;
}

namespace {

// One of `names` (n of them) by the share `u` (0..1).
const char* one_of(const char* const* names, int n, double u) {
  return names[std::min(n - 1, static_cast<int>(u * n))];
}

std::string stack_words(const SkyPick& p) {
  std::string s;
  for (int i = 0; i < p.layer_count; ++i) {
    s += (i == 0                   ? ""
          : i + 1 == p.layer_count ? " and "
                                   : ", ") +
         std::string(p.layers[i].kind->name) + " over " + pct(p.layers[i].cover);
  }
  return s.empty() ? "a clear sky" : s;
}

// THE DAY'S STACK: a low layer of heaps, rolls or towering heaps over a share of the sky the
// texture's own cloud sets (a third of it at the most), sometimes scud racing under it, and most
// often a high layer (ice's hairs, a rippled middle sheet, fine ripples or a few streaks) for the
// sky's depth; every layer in islands, its outlines twisted.
void deal_day(SkyPick& p) {
  auto u = [&](uint32_t k) { return hash01(p.seed, 300 + k); };
  p.layer_count = 0;
  const double share = std::clamp(p.cover, 0.0, 1.0);
  static const char* const kLow[] = {"cumulus", "cumulus", "stratocumulus", "congestus"};
  const char* low = one_of(kLow, 4, u(0));
  const double cover = std::clamp((0.16 + 0.3 * share) * (0.75 + 0.5 * u(1)), 0.1, 0.42);
  add_layer(p, low, cover, 0.3 + 0.3 * u(2), 0.6 + 0.5 * u(3), 0.7 + 0.4 * u(4));
  if (u(5) < 0.35) add_layer(p, "fractus", 0.4 * cover, 0.6, 1.3, 0.8 + 0.4 * u(6));
  static const char* const kHigh[] = {"cirrus", "cirrus", "altocumulus", "cirrocumulus", "wisps"};
  if (u(7) < 0.75) {
    const render::CloudKind* high = render::cloud_kind_named(one_of(kHigh, 5, u(8)));
    add_layer(p, high->name, deal(high->cover, u(9)) * 0.7, 0.5 + 0.3 * u(10), 0.6 + 0.5 * u(11),
              0.8 + 0.4 * u(12));
  }
}

// THE NIGHT'S STACK: the scud racing low across the moon, and one layer over it (broken rolls,
// a rippled middle sheet or high ice), sometimes high streaks over that; the sky between open.
void deal_night(SkyPick& p) {
  auto u = [&](uint32_t k) { return hash01(p.seed, 9 + k); };
  p.layer_count = 0;
  add_layer(p, "fractus", 0.14 + 0.14 * u(0), 0.5 + 0.3 * u(1), 1.0 + 0.5 * u(2), 0.8 + 0.4 * u(3));
  const double r = u(4);
  if (r < 0.45) {
    add_layer(p, "stratocumulus", 0.22 + 0.16 * u(5), 0.55 + 0.25 * u(6), 0.9 + 0.5 * u(7),
              0.7 + 0.4 * u(8));
  } else if (r < 0.7) {
    add_layer(p, "altocumulus", 0.3 + 0.15 * u(5), 0.6, 0.8 + 0.4 * u(7), 0.8 + 0.4 * u(8));
  } else {
    add_layer(p, "cirrus", 0.3 + 0.15 * u(5), 0.5, 0.7 + 0.4 * u(7), 0.8 + 0.4 * u(8));
  }
  if (r < 0.7 && u(9) < 0.4) add_layer(p, "wisps", 0.1 + 0.1 * u(10), 0.4, 0.8, 1);
}

}  // namespace

SkyPick pick_sky(const SkyColours& c, std::string_view map) {
  SkyPick p;
  // The map's own air: its seed, and a wind always blowing (8 to 14 m/s at 10 m).
  p.seed = seed_of(map);
  p.wind = 8 + 6 * hash01(p.seed, 7);
  p.wind_from = 2 * kPi * hash01(p.seed, 8);
  if (!c.found) {
    render::sky_style_named(p.type, p.style);
    deal_day(p);
    p.why = "no sky texture: the Earth's fair day, " + stack_words(p);
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
  p.bright = y;
  deal_day(p);
  p.why = "a " + hue + " sky (" + pct(y) + " bright, " + pct(sat) + " saturated) with " +
          pct(c.cover) + " cloud: " + p.type + ", the sun at " +
          core::num::to_fixed(p.sun_elevation, 0) + " degrees, the sky at " + pct(p.sky_light) +
          " against the rooms, " + stack_words(p);
  return p;
}

bool dark_sky(const SkyPick& p) { return p.bright < kDarkSky; }

void set_cover(SkyPick& p, double cover) {
  if (p.layer_count == 0 || p.layers[0].cover <= 0) return;
  const double k = std::max(cover, 0.0) / p.layers[0].cover;
  for (int i = 0; i < p.layer_count; ++i) p.layers[i].cover = std::min(1.0, p.layers[i].cover * k);
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
  p.wind = 8 + 6 * hash01(p.seed, 7);
  p.wind_from = 2 * kPi * hash01(p.seed, 8);
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
  // The night's stack in place of the day's (and of its words).
  const std::string day = ", " + stack_words(p);
  if (p.why.size() >= day.size() && p.why.compare(p.why.size() - day.size(), day.size(), day) == 0)
    p.why.resize(p.why.size() - day.size());
  deal_night(p);
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
           " lit, a wind of " + core::num::to_fixed(p.wind, 0) + " m/s, " + stack_words(p) +
           ", seen " + core::num::to_fixed(p.sky_seen, 0) + " times brighter";
}

void at_hour(SkyPick& p, const SkyPick& night, double hour) {
  constexpr double kDeclination = 15;  // a summer's day: the sun this far north of the equator
  const core::SunPlace sun = core::sun_at(hour, kLatitude, kDeclination);
  const double elevation = sun.elevation / kDeg, azimuth = sun.azimuth / kDeg;
  if (elevation < -6) {
    // Night: the map's own, the sun under the horizon where the hour puts it and the moon across
    // the sky from it.
    const double turn = azimuth - night.sun_azimuth;
    p = night;
    p.moon_azimuth += turn;
    p.sidereal = std::fmod(night.sidereal + 15 * hour, 360.0);
  } else {
    p.night = false;
  }
  p.sun_elevation = elevation;
  p.sun_azimuth = azimuth;
}

render::Clouds clouds_of(const SkyPick& p, double top_y, double t) {
  render::Clouds c;
  for (int i = 0; i < p.layer_count && c.count < render::kCloudLayers; ++i) {
    if (p.layers[i].kind == nullptr || p.layers[i].cover <= 0) continue;
    deal_layer(p.layers[i], static_cast<uint32_t>(i), p, top_y, t, c.layers[c.count++]);
  }
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

const render::FarLandKind* far_land_for(const SkyPick& pick, std::string_view map,
                                        std::string_view word, bool* known) {
  if (known != nullptr) *known = true;
  if (word == "off") return nullptr;
  if (word != "auto") {
    const render::FarLandKind* k = render::find_far_land(word);
    if (known != nullptr) *known = k != nullptr;
    return k;
  }
  const std::string_view t = pick.type;
  if (t == "dust") return render::find_far_land("sierra");
  if (t == "alien" || t == "violet" || t == "toxic" || t == "nebula")
    return render::find_far_land("xeno");
  if (t == "hell" || t == "ash") return render::find_far_land("ember");
  if (t == "ice") return render::find_far_land("rime");
  // The Earth's air, and a black sky (a plain night's, though it picks the airless type): one of
  // the Earth's lands, dealt off the name.
  std::vector<const render::FarLandKind*> earthly;
  for (const render::FarLandKind& k : render::far_land_kinds()) {
    if (!k.alien) earthly.push_back(&k);
  }
  const double u = hash01(seed_of(map), 61);
  return earthly[std::min(earthly.size() - 1, static_cast<size_t>(u * double(earthly.size())))];
}

render::FarSite far_site_of(const Scene& scene, std::string_view map) {
  render::FarSite s;
  s.x = 0.5 * (scene.mins[0] + scene.maxs[0]);
  s.z = 0.5 * (scene.mins[2] + scene.maxs[2]);
  const double hx = 0.5 * (scene.maxs[0] - scene.mins[0]),
               hz = 0.5 * (scene.maxs[2] - scene.mins[2]);
  s.radius_m = std::sqrt(hx * hx + hz * hz) + 4;
  s.floor_y = scene.mins[1];
  s.seed = 1 + (seed_of(map) % 9973);
  return s;
}

}  // namespace ge::brush::app
