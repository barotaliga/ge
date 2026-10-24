// The sky types and their knobs (render/sky_style.hpp).
//
// ge:summary The sky types' catalogue, the knobs over a type and the frame block's style rows
// ge:keywords render sky sky-type sky-style catalogue knobs style_rows alien hell void dust
// ge:skill render
// ge:test tests/render_sky_style_test.cpp
#include "render/sky_style.hpp"

#include <array>
#include <cctype>
#include <cmath>

#include "render/view.hpp"

namespace ge::render {

namespace {

constexpr double kDeg = 3.14159265358979 / 180.0;

SkyStyle earth() { return SkyStyle{}; }

// A desert planet's dusty air: little Rayleigh and much fine red dust that drinks the blue, so the
// day's sky is butterscotch and the sunset blue round a small sun.
SkyStyle dust() {
  SkyStyle s;
  s.rayleigh[0] = s.rayleigh[1] = s.rayleigh[2] = 0.08;
  s.dust = 8;
  s.dust_height = 10;
  s.dust_g = 0.55;
  s.dust_absorb[0] = 0.1;
  s.dust_absorb[1] = 0.45;
  s.dust_absorb[2] = 1.3;
  s.ozone = 0;
  s.sun_size = 0.65;
  return s;
}

// A green air (its scattering peaks in the green) under two suns, a ringed planet up the sky.
SkyStyle alien() {
  SkyStyle s;
  s.rayleigh[0] = 0.45;
  s.rayleigh[1] = 2.4;
  s.rayleigh[2] = 1.1;
  s.dust = 1.5;
  s.dust_absorb[0] = 0.7;
  s.dust_absorb[1] = 0.4;
  s.dust_absorb[2] = 1.6;
  s.sun2 = 0.6;
  s.sun2_heading = 0.32;
  s.sun2_elevation = 0.06;
  s.sun2_size = 1.7;
  s.sun2_tint[0] = 1.0;
  s.sun2_tint[1] = 0.55;
  s.sun2_tint[2] = 0.3;
  s.planet = 0.11;
  s.planet_heading = 0.9;
  s.planet_elevation = 0.42;
  s.planet_tint[0] = 0.75;
  s.planet_tint[1] = 0.62;
  s.planet_tint[2] = 0.5;
  s.ring = 2.2;
  s.ring_tilt = 0.32;
  s.stars = 0.15;
  return s;
}

// A red air thick with smoke that drinks the green and the blue, a swollen red sun, and the glow
// of fire from under the horizon.
SkyStyle hell() {
  SkyStyle s;
  s.rayleigh[0] = 3.5;
  s.rayleigh[1] = 0.25;
  s.rayleigh[2] = 0.08;
  s.dust = 18;
  s.dust_height = 6;
  s.dust_g = 0.6;
  s.dust_absorb[0] = 0.05;
  s.dust_absorb[1] = 2.2;
  s.dust_absorb[2] = 3.2;
  s.ozone = 0;
  s.sun_size = 2.6;
  s.sun_tint[0] = 1.0;
  s.sun_tint[1] = 0.45;
  s.sun_tint[2] = 0.2;
  s.glow = 0.6;
  s.glow_tint[0] = 1.0;
  s.glow_tint[1] = 0.28;
  s.glow_tint[2] = 0.05;
  s.glow_height = 0.18;
  return s;
}

// No air at all: a black sky, every star by day, a nebula across it and a great ringed planet.
SkyStyle void_sky() {
  SkyStyle s;
  s.rayleigh[0] = s.rayleigh[1] = s.rayleigh[2] = 0;
  s.dust = 0;
  s.ozone = 0;
  s.stars = 1;
  s.nebula = 0.35;
  s.nebula_a[0] = 0.9;
  s.nebula_a[1] = 0.25;
  s.nebula_a[2] = 0.7;
  s.nebula_b[0] = 0.2;
  s.nebula_b[1] = 0.55;
  s.nebula_b[2] = 1.0;
  s.planet = 0.22;
  s.planet_heading = 1.0;
  s.planet_elevation = 0.3;
  s.planet_tint[0] = 0.55;
  s.planet_tint[1] = 0.65;
  s.planet_tint[2] = 0.85;
  s.ring = 2.0;
  s.ring_tilt = 0.25;
  return s;
}

// A thin violet air through which a pink and blue nebula and the brighter stars show by day.
SkyStyle nebula() {
  SkyStyle s;
  s.rayleigh[0] = 0.45;
  s.rayleigh[1] = 0.18;
  s.rayleigh[2] = 0.6;
  s.dust = 0.4;
  s.stars = 0.6;
  s.nebula = 0.5;
  s.nebula_a[0] = 1.0;
  s.nebula_a[1] = 0.3;
  s.nebula_a[2] = 0.65;
  s.nebula_b[0] = 0.35;
  s.nebula_b[1] = 0.45;
  s.nebula_b[2] = 1.0;
  s.nebula_size = 1.3;
  return s;
}

// A violet sky: the red and the blue scattered and the green not, two small pale moons.
SkyStyle violet() {
  SkyStyle s;
  s.rayleigh[0] = 1.2;
  s.rayleigh[1] = 0.45;
  s.rayleigh[2] = 1.3;
  s.dust = 1.2;
  s.planet = 0.035;
  s.planet_heading = 0.7;
  s.planet_elevation = 0.55;
  s.planet_tint[0] = 0.9;
  s.planet_tint[1] = 0.88;
  s.planet_tint[2] = 0.95;
  s.sun2 = 0.25;
  s.sun2_heading = -0.35;
  s.sun2_elevation = 0.25;
  s.sun2_size = 0.6;
  s.sun2_tint[0] = 0.7;
  s.sun2_tint[1] = 0.8;
  s.sun2_tint[2] = 1.0;
  return s;
}

// A pale cyan air over an ice world, a giant ringed planet low over the horizon.
SkyStyle ice() {
  SkyStyle s;
  s.rayleigh[0] = 0.32;
  s.rayleigh[1] = 0.95;
  s.rayleigh[2] = 1.25;
  s.dust = 2.2;
  s.dust_absorb[0] = s.dust_absorb[1] = s.dust_absorb[2] = 0.2;
  s.planet = 0.32;
  s.planet_heading = 1.15;
  s.planet_elevation = 0.2;
  s.planet_tint[0] = 0.85;
  s.planet_tint[1] = 0.92;
  s.planet_tint[2] = 1.0;
  s.ring = 2.3;
  s.ring_tilt = 0.18;
  return s;
}

// A volcanic world's ash: a dim brown air that drinks nearly every colour, a dull sun, a faint
// fire below.
SkyStyle ash() {
  SkyStyle s;
  s.rayleigh[0] = s.rayleigh[1] = s.rayleigh[2] = 0.1;
  s.dust = 22;
  s.dust_height = 8;
  s.dust_g = 0.6;
  s.dust_absorb[0] = 1.0;
  s.dust_absorb[1] = 1.1;
  s.dust_absorb[2] = 1.25;
  s.sun_size = 1.4;
  s.sun_tint[0] = 1.0;
  s.sun_tint[1] = 0.7;
  s.sun_tint[2] = 0.45;
  s.glow = 0.15;
  s.glow_tint[0] = 1.0;
  s.glow_tint[1] = 0.35;
  s.glow_tint[2] = 0.08;
  s.glow_height = 0.08;
  return s;
}

// A sickly yellow-green air, the blue drunk by its haze: a world of slime and fumes.
SkyStyle toxic() {
  SkyStyle s;
  s.rayleigh[0] = 0.6;
  s.rayleigh[1] = 0.9;
  s.rayleigh[2] = 0.12;
  s.dust = 10;
  s.dust_height = 5;
  s.dust_g = 0.65;
  s.dust_absorb[0] = 0.05;
  s.dust_absorb[1] = 0.1;
  s.dust_absorb[2] = 3.0;
  s.ozone = 0.3;
  s.sun_tint[0] = 1.0;
  s.sun_tint[1] = 1.0;
  s.sun_tint[2] = 0.6;
  return s;
}

const std::array<SkyType, 10>& catalogue() {
  static const std::array<SkyType, 10> types = {{
      {"earth", "The Earth's air: blue by day, red at sunset", earth()},
      {"dust", "A desert planet: butterscotch by day, a blue sunset, a small sun", dust()},
      {"alien", "A green air under two suns, a ringed planet up the sky", alien()},
      {"hell", "A red smoke-thick air, a swollen sun, fire glowing from below", hell()},
      {"void", "No air: a black sky, stars by day, a nebula and a ringed planet", void_sky()},
      {"nebula", "A thin violet air, a nebula and the stars showing by day", nebula()},
      {"violet", "A violet sky, a pale small second sun and a small moon", violet()},
      {"ice", "A pale cyan air, a giant ringed planet low over the horizon", ice()},
      {"ash", "A volcanic world's ash: a dim brown air, a dull sun, a faint fire", ash()},
      {"toxic", "A sickly yellow-green air, its blue drunk by the fumes", toxic()},
  }};
  return types;
}

bool same_word(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

}  // namespace

std::span<const SkyType> sky_types() { return catalogue(); }

bool sky_style_named(std::string_view name, SkyStyle& out) {
  for (const SkyType& t : catalogue()) {
    if (same_word(t.name, name)) {
      out = t.style;
      return true;
    }
  }
  return false;
}

std::string_view sky_type_list() {
  return "earth, dust, alien, hell, void, nebula, violet, ice, ash, toxic";
}

void with_knobs(SkyStyle& s, const SkyKnobs& k) {
  if (k.air >= 0) {
    for (double& r : s.rayleigh) r *= k.air;
    s.dust *= k.air;
  }
  if (k.dust >= 0) s.dust = k.dust;
  if (k.sun_size >= 0) s.sun_size = k.sun_size;
  if (k.sun2 >= 0) s.sun2 = k.sun2;
  if (k.planet >= 0) s.planet = k.planet * kDeg;
  if (k.planet_heading >= 0) s.planet_heading = k.planet_heading * kDeg;
  if (k.planet_elevation >= 0) s.planet_elevation = k.planet_elevation * kDeg;
  if (k.ring >= 0) s.ring = k.ring;
  if (k.nebula >= 0) s.nebula = k.nebula;
  if (k.stars >= 0) s.stars = k.stars;
  if (k.glow >= 0) s.glow = k.glow;
}

void style_rows(const SkyStyle& s, const double sun_dir[3], float out[kSkyStyleRows][4]) {
  const auto row = [&](int i, const double rgb[3], double w) {
    for (int c = 0; c < 3; ++c) out[i][c] = static_cast<float>(rgb[c]);
    out[i][3] = static_cast<float>(w);
  };
  row(0, s.rayleigh, s.dust);
  row(1, s.dust_absorb, s.ozone);
  row(2, s.sun_tint, s.sun_size);
  // The second sun stands off the sun by its offsets in heading and elevation.
  const double heading = std::atan2(sun_dir[0], sun_dir[2]);
  const double elevation = std::asin(std::fmax(-1.0, std::fmin(1.0, sun_dir[1])));
  double d[3];
  look_dir(heading + s.sun2_heading, elevation + s.sun2_elevation, d);
  row(3, d, s.sun2);
  row(4, s.sun2_tint, s.sun2_size);
  look_dir(heading + s.planet_heading, s.planet_elevation, d);
  row(5, d, s.planet);
  row(6, s.planet_tint, s.ring);
  row(7, s.nebula_a, s.nebula);
  row(8, s.nebula_b, s.nebula_size);
  double glow[3];
  for (int c = 0; c < 3; ++c) glow[c] = s.glow_tint[c] * s.glow;
  row(9, glow, s.glow_height);
  out[10][0] = static_cast<float>(s.ring_tilt);
  out[10][1] = static_cast<float>(s.stars);
  out[10][2] = static_cast<float>(s.dust_g);
  out[10][3] = static_cast<float>(s.dust_height);
}

bool same_air(const SkyStyle& a, const SkyStyle& b) {
  for (int c = 0; c < 3; ++c) {
    if (a.rayleigh[c] != b.rayleigh[c] || a.dust_absorb[c] != b.dust_absorb[c]) return false;
  }
  return a.dust == b.dust && a.ozone == b.ozone && a.dust_g == b.dust_g &&
         a.dust_height == b.dust_height;
}

}  // namespace ge::render
