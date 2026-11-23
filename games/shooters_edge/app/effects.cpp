// ge:summary Shooter's Edge's effects: each trail and burst's particles, the lights, the blend
// ge:keywords effects particles trail explosion speck palette ramp blood teleport lava screen-blend
// ge:keywords dust dust_off dust_in ash flakes warp_field
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "classic/protocol/directions.hpp"
#include "core/prng.hpp"

namespace shooters_edge::effects {

namespace {

using ge::core::Rng;
using ge::render::Particle;

constexpr double kUnit = 0.025;                                    // a map unit, m
constexpr float kFall = float(40 * kUnit / ge::render::kGravity);  // 40 units/s^2 of gravity
constexpr float kSpeck = 0.05f;  // a speck across, m: about the classic dot's size

// The palette's ramps the classic specks run along (indices into the pack's palette): an
// explosion's bright yellow to dark brown, its second ramp slower, the fire ramp to its greys.
constexpr int kRamp1[8] = {111, 109, 107, 105, 103, 101, 99, 97};
constexpr int kRamp2[8] = {111, 110, 109, 108, 107, 106, 104, 102};
constexpr int kRamp3[6] = {109, 107, 6, 5, 4, 3};

double rnd(Rng& r, double lo, double hi) { return ge::core::range(r, lo, hi); }
int pick(Rng& r, int n) { return std::min(int(rnd(r, 0, n)), n - 1); }

double to_linear(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }

float grey_of(const float rgb[3]) { return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2]; }

Particle at(const double p[3]) {
  Particle q{};
  for (int k = 0; k < 3; ++k) q.p[k] = float(p[k]);
  return q;
}

// An unlit speck of palette entry `index`, its colour drifting toward `to`'s grey by e every
// `drift` s (0: steady), alive `life` s.
Particle speck(const double p[3], Palette pal, int index, int to, double drift, double life) {
  Particle q = at(p);
  palette_rgb(pal, index, q.tint);
  float end[3];
  palette_rgb(pal, to, end);
  q.albedo = grey_of(end);
  q.kelvin = -1;  // shining, as a fullbright texel
  q.cool_s = float(drift);
  q.life = float(life);
  q.size = kSpeck;
  q.opacity = 1;
  q.fall = kFall;
  return q;
}

// A point jittered by up to `j` units each way.
void jitter(Rng& r, const double p[3], double j, double out[3]) {
  for (int k = 0; k < 3; ++k) out[k] = p[k] + rnd(r, -j, j) * kUnit;
}

void trail(const double from[3], const double to[3], uint32_t seed, bool rocket, Palette pal,
           Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  const int n = std::max(1, int(len / kTrailStep));
  for (int i = 0; i < n; ++i) {
    const double t = (i + rnd(r, 0, 1)) / n;
    const double p[3] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t};
    double q[3];
    jitter(r, p, 3, q);
    // Along the fire ramp at 5 steps a second, from its start (a rocket's at its orange, a
    // grenade's at its greys), rising as warm air does.
    const int r0 = rocket ? pick(r, 4) : 2 + pick(r, 4);
    Particle s = speck(q, pal, kRamp3[r0], kRamp3[5], rocket && r0 < 2 ? 0.3 : 0, (6 - r0) / 5.0);
    s.fall = -kFall;
    out->push_back(s);
  }
}

}  // namespace

void palette_rgb(Palette pal, int index, float out[3]) {
  index = std::clamp(index, 0, 255);
  for (int k = 0; k < 3; ++k) {
    const double c = pal.size() >= 768 ? pal[size_t(index) * 3 + size_t(k)] / 255.0 : index / 255.0;
    out[k] = float(to_linear(c));
  }
}

void rocket_trail(const double from[3], const double to[3], uint32_t seed, Palette pal,
                  Particles* out) {
  trail(from, to, seed, true, pal, out);
}

void grenade_trail(const double from[3], const double to[3], uint32_t seed, Palette pal,
                   Particles* out) {
  trail(from, to, seed, false, pal, out);
}

void explosion(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 1024; ++i) {
    double q[3];
    jitter(r, p, 16, q);
    const int r0 = pick(r, 4);
    // Half on the bright ramp at 10 steps a second, speeding out (x e^4t); half on the second
    // ramp at 15 a second, slowing (x e^-t).
    const bool fast = (i & 1) != 0;
    const double life = (8 - r0) / (fast ? 10.0 : 15.0);
    Particle s = fast ? speck(q, pal, kRamp1[r0], kRamp1[7], life * 0.45, life)
                      : speck(q, pal, kRamp2[r0], kRamp2[7], life * 0.6, life);
    for (int k = 0; k < 3; ++k) s.v[k] = float(rnd(r, -256, 256) * kUnit);
    s.drag = fast ? -4.0f : 1.0f;
    out->push_back(s);
  }
}

void explosion2(const double p[3], int start, int length, uint32_t seed, Palette pal,
                Particles* out) {
  if (length <= 0) return;
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 512; ++i) {
    double q[3];
    jitter(r, p, 16, q);
    Particle s = speck(q, pal, (start + i % length) & 255, 0, 0, 0.3);
    for (int k = 0; k < 3; ++k) s.v[k] = float(rnd(r, -256, 256) * kUnit);
    s.drag = -4.0f;
    out->push_back(s);
  }
}

void burst(const double p[3], int specks, int colour, const double way[3], uint32_t seed,
           Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < specks; ++i) {
    double q[3];
    jitter(r, p, 8, q);
    Particle s = speck(q, pal, (colour & ~7) + pick(r, 8), 0, 0, 0.1 * pick(r, 5) + 0.02);
    for (int k = 0; k < 3; ++k) s.v[k] = float(way[k]);
    out->push_back(s);
  }
}

void impact(const double p[3], int specks, uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double spread = specks > 130 ? 24 : specks > 20 ? 16 : 8;
  for (int i = 0; i < specks; ++i) {
    double q[3];
    jitter(r, p, spread, q);
    out->push_back(speck(q, pal, pick(r, 8), 0, 0, 0.1 * pick(r, 5) + 0.02));
  }
}

void blood(const double p[3], int specks, bool arc, uint32_t seed, Palette pal, Particles* out,
           const double way[3]) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < specks; ++i) {
    double q[3];
    jitter(r, p, 8, q);
    Particle s = speck(q, pal, (arc ? 224 : 72) + pick(r, 8), 0, 0, 0.1 * pick(r, 5) + 0.02);
    if (way != nullptr)
      for (int k = 0; k < 3; ++k) s.v[k] = float(way[k]);
    out->push_back(s);
  }
}

double blood_trail(const double from[3], const double to[3], double step, double carry,
                   uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  // The specks' places along the stretch: carry, carry + step, ... short of its length.
  const int n = len > carry ? static_cast<int>((len - carry) / step) + 1 : 0;
  for (int i = 0; i < n; ++i) {
    const double t = (carry + step * i) / len;
    const double p[3] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t};
    double q[3];
    jitter(r, p, 3, q);
    out->push_back(speck(q, pal, 67 + pick(r, 4), 0, 0, 2));
  }
  return carry + step * n - len;
}

void teleport_splash(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = -16; i < 16; i += 4)
    for (int j = -16; j < 16; j += 4)
      for (int k = -24; k < 32; k += 4) {
        const double q[3] = {p[0] + (j + rnd(r, 0, 4)) * kUnit, p[1] + (k + rnd(r, 0, 4)) * kUnit,
                             p[2] + (i + rnd(r, 0, 4)) * kUnit};
        double d[3] = {8.0 * j, 8.0 * k, 8.0 * i};
        const double l = std::max(std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), 1e-6);
        Particle s = speck(q, pal, 7 + pick(r, 8), 0, 0, 0.2 + 0.02 * pick(r, 8));
        const double v = rnd(r, 50, 114) * kUnit;
        for (int c = 0; c < 3; ++c) s.v[c] = float(d[c] / l * v);
        out->push_back(s);
      }
}

namespace {

// A speck of Quake II's at `p` + (x, y, z) map units (x, y the plan, z up), its velocity (x, y,
// z) units/s, falling at `g` units/s^2.
Particle q2_speck(const double p[3], double x, double y, double z, const double v[3], double g,
                  Palette pal, int index, double life) {
  const double q[3] = {p[0] + x * kUnit, p[1] + z * kUnit, p[2] + y * kUnit};
  Particle s = speck(q, pal, index, 0, 0, life);
  s.v[0] = float(v[0] * kUnit);
  s.v[1] = float(v[2] * kUnit);  // the plan is x/z, y up
  s.v[2] = float(v[1] * kUnit);
  s.fall = float(g / 40) * kFall;
  return s;
}

}  // namespace

void q2_teleport_burst(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = -16; i <= 16; i += 4)
    for (int j = -16; j <= 16; j += 4)
      for (int k = -16; k <= 32; k += 4) {
        const double l = std::sqrt(double(i * i + j * j + k * k));
        const double speed = 50 + pick(r, 64);
        double v[3] = {0, 0, 0};
        if (l > 0) {  // the plan's two axes swapped in the way it flies
          v[0] = j / l * speed;
          v[1] = i / l * speed;
          v[2] = k / l * speed;
        }
        const double x = i + pick(r, 4), y = j + pick(r, 4), z = k + pick(r, 4);
        out->push_back(q2_speck(p, x, y, z, v, 80, pal, 7 + pick(r, 8), 0.30 + 0.02 * pick(r, 8)));
      }
}

void q2_boss_teleport(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  constexpr int kColours[4] = {16, 104, 168, 144};
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 4096; ++n) {
    const double a = 2 * 3.14159265358979 * pick(r, 1024) / 1023, c = std::cos(a), sn = std::sin(a);
    const double d = pick(r, 32), out_v = 70 + pick(r, 64);
    const double v[3] = {c * out_v, sn * out_v, -100.0 + pick(r, 32)};
    const double life = (0.5 + 0.3 * rnd(r, 0, 1)) / 0.3;
    Particle s = q2_speck(p, c * d, sn * d, 8 + pick(r, 90), v, 0, pal, kColours[pick(r, 4)], life);
    s.accel[0] = float(-c * 100 * kUnit);  // the plan is x/z, y up
    s.accel[1] = float(160 * kUnit);
    s.accel[2] = float(-sn * 100 * kUnit);
    out->push_back(s);
  }
}

void q2_item_back(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 64; ++n) {
    const double x = rnd(r, -8, 8), y = rnd(r, -8, 8), z = rnd(r, -8, 8);
    const double v[3] = {rnd(r, -8, 8), rnd(r, -8, 8), rnd(r, -8, 8)};
    out->push_back(q2_speck(p, x, y, z, v, 16, pal, 212 + pick(r, 4), rnd(r, 1.0, 1.3)));
  }
}

void q2_specks(const double p[3], const double normal[3], int count, int colour, int reach,
               bool vary, uint32_t seed, Palette pal, Particles* out, double fall) {
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < count; ++n) {
    const double d = pick(r, std::max(reach, 1));  // out along the face, the same on every axis
    const double x = rnd(r, -4, 4) + d * normal[0], y = rnd(r, -4, 4) + d * normal[1],
                 z = rnd(r, -4, 4) + d * normal[2];
    const double v[3] = {rnd(r, -20, 20), rnd(r, -20, 20), rnd(r, -20, 20)};
    const int index = vary ? (colour + pick(r, 8)) & 0xff : colour & 0xff;
    out->push_back(q2_speck(p, x, y, z, v, fall, pal, index, rnd(r, 0.5, 0.8)));
  }
}

void q2_player_flash(const double p[3], Q2Arrival kind, uint32_t seed, Palette pal,
                     Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const int base = kind == kSqLogin ? 208 : kind == kSqLogout ? 64 : 224;
  for (int n = 0; n < 500; ++n) {
    const double x = rnd(r, -16, 16), y = rnd(r, -16, 16), z = rnd(r, -24, 32);
    const double v[3] = {rnd(r, -20, 20), rnd(r, -20, 20), rnd(r, -20, 20)};
    out->push_back(q2_speck(p, x, y, z, v, 80, pal, base + pick(r, 8), rnd(r, 1.0, 1.3)));
  }
}

void q2_fountain(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 8; ++n) {
    const double x = rnd(r, -16, 16), y = rnd(r, -16, 16), z = -1 - pick(r, 8);
    const double v[3] = {0, 0, 80.0 + pick(r, 8)};
    out->push_back(q2_speck(p, x, y, z, v, 80, pal, 219, 2));
  }
}

void q2_blaster_specks(const double p[3], const double normal[3], uint32_t seed, Palette pal,
                       Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 40; ++n) {
    const double d = pick(r, 16);  // out along the face, the same on every axis
    const double x = pick(r, 8) - 4 + d * normal[0], y = pick(r, 8) - 4 + d * normal[1],
                 z = pick(r, 8) - 4 + d * normal[2];
    const double v[3] = {normal[0] * 30 + rnd(r, -40, 40), normal[1] * 30 + rnd(r, -40, 40),
                         normal[2] * 30 + rnd(r, -40, 40)};
    out->push_back(q2_speck(p, x, y, z, v, 80, pal, (0xe0 + pick(r, 8)) & 0xff, rnd(r, 0.5, 0.8)));
  }
}

void q2_blast_specks(const double p[3], int colour, uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 256; ++n) {
    const double x = pick(r, 32) - 16, y = pick(r, 32) - 16, z = pick(r, 32) - 16;
    const double v[3] = {pick(r, 384) - 192.0, pick(r, 384) - 192.0, pick(r, 384) - 192.0};
    out->push_back(
        q2_speck(p, x, y, z, v, 80, pal, (colour + pick(r, 8)) & 0xff, rnd(r, 0.625, 1.0)));
  }
}

namespace {

// The stretch `from` .. `to` (engine metres) in map units' axes (x, y the plan, z up): its
// length in units and its way, a unit long.
double q2_stretch(const double from[3], const double to[3], double way[3]) {
  const double d[3] = {(to[0] - from[0]) / kUnit, (to[2] - from[2]) / kUnit,
                       (to[1] - from[1]) / kUnit};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (int k = 0; k < 3; ++k) way[k] = len > 1e-9 ? d[k] / len : 0;
  return len;
}

}  // namespace

void q2_bubbles(const double from[3], const double to[3], bool second, uint32_t seed, Palette pal,
                Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  const double step = second ? 8 : 32, drift = second ? 10 : 5, rise = second ? 20 : 6;
  for (int i = 0; i * step < len; ++i) {
    const double d = i * step;
    const double x = d * way[0] + rnd(r, -2, 2), y = d * way[1] + rnd(r, -2, 2);
    const double z = d * way[2] + rnd(r, -2, 2) - (second ? 4 : 0);
    const double v[3] = {rnd(r, -drift, drift), rnd(r, -drift, drift),
                         rnd(r, -drift, drift) + rise};
    const double life = second ? rnd(r, 1.0, 1.1) : rnd(r, 1.0, 1.2);
    out->push_back(q2_speck(from, x, y, z, v, 0, pal, 4 + pick(r, 8), life));
  }
}

void q2_force_wall(const double from[3], const double to[3], int colour, uint32_t seed, Palette pal,
                   Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  for (int i = 0; i * 4 < len; ++i) {
    const double d = i * 4;
    if (rnd(r, 0, 1) <= 0.3) continue;
    const double x = d * way[0] + rnd(r, -3, 3), y = d * way[1] + rnd(r, -3, 3),
                 z = d * way[2] + rnd(r, -3, 3);
    const double v[3] = {0, 0, -40 - rnd(r, -10, 10)};
    out->push_back(q2_speck(from, x, y, z, v, 0, pal, colour & 0xff, rnd(r, 3.0, 3.5)));
  }
}

void q2_steam(const double p[3], const double jet[3], int count, int colour, double fall,
              uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double mag = std::sqrt(jet[0] * jet[0] + jet[1] * jet[1] + jet[2] * jet[2]);
  if (mag <= 0) return;
  const double d[3] = {jet[0] / mag, jet[1] / mag, jet[2] / mag};
  // Two axes across the jet: one off the axis it is least along, then their cross product.
  const double a[3] = {std::abs(d[0]) < 0.9 ? 1.0 : 0.0, std::abs(d[0]) < 0.9 ? 0.0 : 1.0, 0};
  double u[3] = {d[1] * a[2] - d[2] * a[1], d[2] * a[0] - d[0] * a[2], d[0] * a[1] - d[1] * a[0]};
  const double ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  for (double& c : u) c /= ul;
  const double w[3] = {d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2],
                       d[0] * u[1] - d[1] * u[0]};
  for (int n = 0; n < count; ++n) {
    const double x = mag * 0.1 * rnd(r, -1, 1), y = mag * 0.1 * rnd(r, -1, 1),
                 z = mag * 0.1 * rnd(r, -1, 1);
    const double cu = rnd(r, -1, 1) * mag / 3, cw = rnd(r, -1, 1) * mag / 3;
    double v[3];
    for (int k = 0; k < 3; ++k) v[k] = d[k] * mag + cu * u[k] + cw * w[k];
    out->push_back(
        q2_speck(p, x, y, z, v, fall, pal, (colour + pick(r, 8)) & 0xff, rnd(r, 0.5, 0.8)));
  }
}

void q2_blue_hit(const double p[3], const double way[3], uint32_t seed, Palette pal,
                 Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 40; ++n) {
    const double d = pick(r, 16);
    const double x = pick(r, 8) - 4 + d * way[0], y = pick(r, 8) - 4 + d * way[1],
                 z = pick(r, 8) - 4 + d * way[2];
    double v[3];
    for (int k = 0; k < 3; ++k) v[k] = way[k] * 30 + rnd(r, -40, 40);
    out->push_back(q2_speck(p, x, y, z, v, 80, pal, 224 + pick(r, 8), rnd(r, 0.5, 0.8)));
  }
}

void q2_widow_splash(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  constexpr int kColours[4] = {16, 104, 168, 144};
  Rng r = ge::core::make_rng(seed);
  for (int n = 0; n < 256; ++n) {
    // A way uniform on the sphere: a height and a turn.
    const double h = rnd(r, -1, 1), turn = rnd(r, 0, 2 * 3.14159265358979323846);
    const double flat = std::sqrt(std::max(0.0, 1 - h * h));
    const double d[3] = {flat * std::cos(turn), flat * std::sin(turn), h};
    const double v[3] = {d[0] * 40, d[1] * 40, d[2] * 40};
    out->push_back(q2_speck(p, d[0] * 45, d[1] * 45, d[2] * 45, v, 0, pal, kColours[pick(r, 4)],
                            rnd(r, 0.625, 1.0)));
  }
}

void q2_column_trail(const double from[3], const double to[3], int colour, uint32_t seed,
                     Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  for (int i = 0; i * 5 < len; ++i) {
    const double d = i * 5;
    const double x = d * way[0] + rnd(r, -16, 16), y = d * way[1] + rnd(r, -16, 16),
                 z = d * way[2] + rnd(r, -16, 16);
    const double v[3] = {rnd(r, -5, 5), rnd(r, -5, 5), rnd(r, -5, 5)};
    out->push_back(q2_speck(from, x, y, z, v, 0, pal, colour, rnd(r, 0.8, 1.0)));
  }
}

void q2_ion_trail(const double from[3], const double to[3], int& side, uint32_t seed, Palette pal,
                  Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  for (int i = 0; i * 5 < len; ++i) {
    const double d = i * 5;
    side = -side;
    const double v[3] = {10.0 * (side < 0 ? -1 : 1), 0, 0};
    out->push_back(q2_speck(from, d * way[0], d * way[1], d * way[2], v, 0, pal, 228 + pick(r, 4),
                            rnd(r, 0.15, 0.25)));
  }
}

constexpr double kOneFrameS = 1.0 / 60;  // a speck drawn for one frame

void q2_trap(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 13; ++i) {
    const double v[3] = {rnd(r, -15, 15), rnd(r, -15, 15), rnd(r, -15, 15)};
    out->push_back(q2_speck(p, rnd(r, -1, 1), rnd(r, -1, 1), -14 + 5 * i + rnd(r, -1, 1), v, -80,
                            pal, 224, rnd(r, 0.3, 0.5)));
  }
  for (int i = 0; i < 8; ++i) {
    const double o[3] = {(i & 1) != 0 ? 2.0 : -2.0, (i & 2) != 0 ? 2.0 : -2.0,
                         (i & 4) != 0 ? 2.0 : -2.0};
    // Thrown along the offset with x and y swapped, 50 to 113 units a second.
    const double len = std::sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
    const double speed = 50 + pick(r, 64);
    const double v[3] = {o[1] / len * speed, o[0] / len * speed, o[2] / len * speed};
    out->push_back(q2_speck(p, o[0] + pick(r, 24) * rnd(r, -1, 1),
                            o[1] + pick(r, 24) * rnd(r, -1, 1), o[2] + pick(r, 24) * rnd(r, -1, 1),
                            v, 80, pal, 224 + pick(r, 4), 0.30 + 0.02 * pick(r, 8)));
  }
}

void q2_tracker_trail(const double from[3], const double to[3], uint32_t seed, Palette pal,
                      Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  // "Up" square to the flight: the world's up less its share along the way, else the world's y.
  double up[3] = {-way[2] * way[0], -way[2] * way[1], 1 - way[2] * way[2]};
  double ul = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
  if (ul < 1e-6) up[0] = 0, up[1] = 1, up[2] = 0, ul = 1;
  for (double& c : up) c /= ul;
  const double v[3] = {0, 0, 5};
  int side = pick(r, 2) != 0 ? 1 : -1;
  for (int i = 0; i * 3 < len; ++i) {
    const double d = i * 3.0;
    const double off = side * 8 * std::cos(d * 3.14159265358979323846 / 64);
    side = -side;
    out->push_back(q2_speck(from, d * way[0] + off * up[0], d * way[1] + off * up[1],
                            d * way[2] + off * up[2], v, 0, pal, 0, 0.5));
  }
}

void q2_tracker_shell(const double p[3], double radius_u, uint32_t seed, Palette pal,
                      Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double zero[3] = {0, 0, 0};
  for (int n = 0; n < 300; ++n) {
    const double h = rnd(r, -1, 1), turn = rnd(r, 0, 2 * 3.14159265358979323846);
    const double flat = std::sqrt(std::max(0.0, 1 - h * h));
    out->push_back(q2_speck(p, flat * std::cos(turn) * radius_u, flat * std::sin(turn) * radius_u,
                            h * radius_u, zero, 0, pal, 0, kOneFrameS));
  }
}

namespace {
// `n` specks on a sphere of `radius_u` round `p`, moving out at `speed` units a second, each of a
// colour `colour(r)` and a life `life(r)`.
template <class Colour, class Life>
void q2_sphere(const double p[3], int n, double radius_u, double speed, uint32_t seed, Palette pal,
               Particles* out, Colour colour, Life life) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < n; ++i) {
    const double h = rnd(r, -1, 1), turn = rnd(r, 0, 2 * 3.14159265358979323846);
    const double flat = std::sqrt(std::max(0.0, 1 - h * h));
    const double d[3] = {flat * std::cos(turn), flat * std::sin(turn), h};
    const double v[3] = {d[0] * speed, d[1] * speed, d[2] * speed};
    const int c = colour(r);
    out->push_back(
        q2_speck(p, d[0] * radius_u, d[1] * radius_u, d[2] * radius_u, v, 0, pal, c, life(r)));
  }
}
}  // namespace

void q2_power_splash(const double p[3], double radius_u, uint32_t seed, Palette pal,
                     Particles* out) {
  q2_sphere(
      p, 256, radius_u, 40, seed, pal, out, [](Rng& r) { return 208 + pick(r, 4); },
      [](Rng& r) { return rnd(r, 0.5, 0.8); });
}

void q2_beam_out(const double p[3], double ratio, uint32_t seed, Palette pal, Particles* out) {
  constexpr int kColours[4] = {16, 104, 168, 144};
  q2_sphere(
      p, 300, 45 * ratio, 0, seed, pal, out, [&](Rng& r) { return kColours[pick(r, 4)]; },
      [](Rng&) { return kOneFrameS; });
}

Light q2_point_light(const double p[3], double radius_u, double red, double green, double blue) {
  Light l;
  for (int k = 0; k < 3; ++k) l.pos[k] = p[k];
  l.radius = radius_u * kUnit;
  const double level = radius_u / 128;
  l.colour[0] = red * level, l.colour[1] = green * level, l.colour[2] = blue * level;
  return l;
}

void q2_trail(const double from[3], const double to[3], Q2Trail kind, int& count, uint32_t seed,
              Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  // The spread, by how far it has flown when this frame starts: widest at first.
  const double spread = count > 900 ? 4 : count > 800 ? 2 : 1;
  const double drift = count > 900 ? 15 : count > 800 ? 10 : 5;
  const bool blood = kind != Q2Trail::smoke;
  const int reds = kind == Q2Trail::green_blood ? 219 : 232;  // a green gib's 219..226
  const int tries = static_cast<int>(std::ceil(len / kSqTrailAttemptUnits));
  for (int k = 0; k < tries; ++k) {
    const double gone = k * kSqTrailAttemptUnits;
    const bool shown = pick(r, 1024) < count;
    count = std::max(kSqTrailFloor, count - kSqTrailFade);
    if (!shown) continue;
    const double x = gone * way[0] + rnd(r, -spread, spread);
    const double y = gone * way[1] + rnd(r, -spread, spread);
    const double z = gone * way[2] + rnd(r, -spread, spread);
    double v[3] = {rnd(r, -drift, drift), rnd(r, -drift, drift), rnd(r, -drift, drift)};
    if (blood) {
      v[2] -= 40;  // thrown down, then no gravity
      out->push_back(q2_speck(from, x, y, z, v, 0, pal, reds + pick(r, 8), rnd(r, 1.0, 1.4)));
    } else {  // smoke, rising
      out->push_back(q2_speck(from, x, y, z, v, -40, pal, 4 + pick(r, 8), rnd(r, 1.0, 1.2)));
    }
  }
}

void q2_rocket_fire(const double from[3], const double to[3], uint32_t seed, Palette pal,
                    Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  for (int k = 0; k < static_cast<int>(std::ceil(len)); ++k) {
    const double gone = k;
    if (pick(r, 8) != 0) continue;
    const double x = gone * way[0] + rnd(r, -5, 5), y = gone * way[1] + rnd(r, -5, 5),
                 z = gone * way[2] + rnd(r, -5, 5);
    const double v[3] = {rnd(r, -20, 20), rnd(r, -20, 20), rnd(r, -20, 20)};
    out->push_back(q2_speck(from, x, y, z, v, 80, pal, 220 + pick(r, 4), rnd(r, 1.0, 1.2)));
  }
}

void q2_bolt_trail(const double from[3], const double to[3], uint32_t seed, Palette pal,
                   Particles* out) {
  Rng r = ge::core::make_rng(seed);
  double way[3];
  const double len = q2_stretch(from, to, way);
  for (int k = 0; k < static_cast<int>(std::ceil(len / 5)); ++k) {
    const double gone = 5.0 * k;
    const double x = gone * way[0] + rnd(r, -1, 1), y = gone * way[1] + rnd(r, -1, 1),
                 z = gone * way[2] + rnd(r, -1, 1);
    const double v[3] = {rnd(r, -5, 5), rnd(r, -5, 5), rnd(r, -5, 5)};
    out->push_back(q2_speck(from, x, y, z, v, 0, pal, 224, rnd(r, 0.3, 0.5)));
  }
}

int q2_flies_count(double since_s) {
  const auto n = static_cast<int64_t>(since_s * 1000);
  constexpr int64_t kRamp = 20000, kCycle = 60000;
  if (n < kRamp) return static_cast<int>(std::max<int64_t>(0, n) * kSqFliesMax / kRamp);
  const int64_t left = kCycle - n;
  if (left < kRamp) return static_cast<int>(std::max<int64_t>(0, left) * kSqFliesMax / kRamp);
  return kSqFliesMax;
}

void q2_flies(const double at[3], double t, double since_s, double life, Palette pal,
              Particles* out) {
  // Each fly's two turning rates (0 to 2.55 radians a second), the same for every corpse.
  static const auto kRates = [] {
    std::array<std::array<double, 2>, kSqFliesMax> rates{};
    Rng r = ge::core::make_rng(0x666c6965u);
    for (auto& f : rates)
      for (double& v : f) v = pick(r, 256) * 0.01;
    return rates;
  }();
  const auto& dirs = ge::classic::protocol::directions();
  const int count = q2_flies_count(since_s);
  for (int i = 0; i < count; i += 2) {
    const double a = kRates[size_t(i)][0], b = kRates[size_t(i)][1];
    const double yaw = t * a, pitch = t * b;
    const double cp = std::cos(pitch), sp = std::sin(pitch), cy = std::cos(yaw), sy = std::sin(yaw);
    const double fwd[3] = {cp * cy, cp * sy, -sp};
    const double swing = 64 * std::sin(t + i), swing_v = 64 * std::cos(t + i);
    const auto& n = dirs[size_t(i)];
    // Where it is and how fast it goes there (so a speck living a frame or two stays on its path).
    const double fwd_v[3] = {-sp * b * cy - cp * sy * a, -sp * b * sy + cp * cy * a, -cp * b};
    double p[3], v[3];
    for (int k = 0; k < 3; ++k) {
      p[k] = n[size_t(k)] * swing + fwd[k] * 16;
      v[k] = n[size_t(k)] * swing_v + fwd_v[k] * 16;
    }
    out->push_back(q2_speck(at, p[0], p[1], p[2], v, 0, pal, 0, life));
  }
}

void lava_splash(const double p[3], uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = -16; i < 16; ++i)
    for (int j = -16; j < 16; ++j) {
      const double dx = j * 8 + rnd(r, 0, 8), dz = i * 8 + rnd(r, 0, 8);
      const double q[3] = {p[0] + dx * kUnit, p[1] + rnd(r, 0, 64) * kUnit, p[2] + dz * kUnit};
      Particle s = speck(q, pal, 224 + pick(r, 8), 0, 0, 2 + 0.02 * pick(r, 32));
      const double l = std::sqrt(dx * dx + dz * dz + 256.0 * 256.0);
      const double v = rnd(r, 50, 114) * kUnit;
      s.v[0] = float(dx / l * v);
      s.v[1] = float(256.0 / l * v);
      s.v[2] = float(dz / l * v);
      out->push_back(s);
    }
}

namespace {

// A classic dynamic light of `radius_u` map units: as strong at its middle as the classic's
// (its radius over 128 of the lightmaps' units, a white a shade warm).
Light classic_light(const double p[3], double radius_u) {
  Light l;
  for (int k = 0; k < 3; ++k) l.pos[k] = p[k];
  l.radius = radius_u * kUnit;
  const double level = radius_u / 128;
  l.colour[0] = level, l.colour[1] = 0.95 * level, l.colour[2] = 0.85 * level;
  return l;
}

}  // namespace

Light muzzle_light(const double p[3]) { return classic_light(p, 200); }

Light q2_arrival_light(const double p[3], Q2Arrival kind, uint32_t seed) {
  Rng r = ge::core::make_rng(seed);
  Light l = classic_light(p, 200 + pick(r, 32));
  const double level = l.colour[0];
  l.colour[0] = kind == kSqLogin ? 0 : level;
  l.colour[1] = kind == kSqLogout ? 0 : level;
  l.colour[2] = 0;
  return l;
}

Light rocket_light(const double p[3]) {
  Light l = classic_light(p, 200);
  l.group = kLightRocket;
  return l;
}

bool explosion_light(const double p[3], double age, Light* out) {
  if (age < 0 || age >= kExplosionLight) return false;
  *out = classic_light(p, 350 - 300 * age);  // 350 units, shrinking 300 a second
  out->group = kLightExplosion;
  return true;
}

void dynamic_light_at(std::span<const Light> lights, const double w[3], double strength,
                      double rgb[3], double toward[3], int self) {
  for (int c = 0; c < 3; c++) rgb[c] = toward[c] = 0;
  constexpr double kLevelLuxel = 128.0 / 255.0;  // a light's level of 1, in the probe's units
  double way[3] = {0, 0, 0};
  for (const Light& l : lights) {
    if (self != 0 && l.entity == self) continue;
    const double v[3] = {l.pos[0] - w[0], l.pos[1] - w[1], l.pos[2] - w[2]};
    const double dist = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l.radius <= 0 || dist >= l.radius) continue;
    const double f = (1 - dist / l.radius) * strength * l.boost * kLevelLuxel;
    double lum = 0;
    for (int c = 0; c < 3; c++) {
      rgb[c] += l.colour[c] * f;
      lum += l.colour[c] * f;
    }
    if (dist > 1e-6)
      for (int c = 0; c < 3; c++) way[c] += v[c] / dist * lum;
  }
  const double len = std::sqrt(way[0] * way[0] + way[1] * way[1] + way[2] * way[2]);
  if (len > 1e-9)
    for (int c = 0; c < 3; c++) toward[c] = way[c] / len;
}

double fade_hurt(double hurt, double dt) { return std::max(hurt - kHurtFade * dt, 0.0); }

double take_hurt(double hurt, double damage) { return std::min(hurt + 3 * damage, 150.0); }

void take_powers(Screen& s, int items) {
  s.power = (items & kItemQuad) != 0;
  s.shield = (items & kItemPent) != 0;
  s.suit = (items & kItemSuit) != 0;
  s.stealth = (items & kItemRing) != 0;
  s.powerup = s.power     ? Powerup::quad
              : s.suit    ? Powerup::suit
              : s.stealth ? Powerup::ring
              : s.shield  ? Powerup::pent
                          : Powerup::none;
}

void take_classic_hurt(double& strength, double rgb[3], double armour, double blood) {
  const double count = std::max(kHurtCountMin, 0.5 * armour + 0.5 * blood);
  strength = std::min(strength + 3 * count, kClassicHurtMost);
  const double tint[3][3] = {{200, 100, 100}, {220, 50, 50}, {255, 0, 0}};
  const int kind = armour > blood ? 0 : armour > 0 ? 1 : 2;
  for (int c = 0; c < 3; c++) rgb[c] = tint[kind][c];
}

double fade_bonus(double bonus, double dt, int64_t taken) {
  if (taken > 0) return kBonusFlash;
  return bonus > kBonusFade * dt ? bonus - kBonusFade * dt : 0;
}

void screen_blend(const Screen& s, double rgba[4], const BlendShares& k) {
  // Each layer mixed over the last: the medium, the powers, then the hurt on top.
  double c[3] = {0, 0, 0}, a = 0;
  const auto over = [&](double r, double g, double b, double share) {
    if (share <= 0) return;
    const double na = a + share * (1 - a);
    const double w = share / na;
    c[0] += (r - c[0]) * w;
    c[1] += (g - c[1]) * w;
    c[2] += (b - c[2]) * w;
    a = na;
  };
  if (s.classic) {
    // The original's four tints, colours of 255 at strengths of 255, in its order.
    const auto tint = [&](double r, double g, double b, double strength, double share) {
      over(r / 255, g / 255, b / 255, std::min(strength / 255, 1.0) * share);
    };
    switch (s.medium) {
      case Medium::water:
        tint(130, 80, 50, 128, k.content);
        break;
      case Medium::slime:
        tint(0, 25, 5, 150, k.content);
        break;
      case Medium::lava:
        tint(255, 80, 0, 150, k.content);
        break;
      case Medium::air:
        tint(s.air_rgb[0], s.air_rgb[1], s.air_rgb[2], s.air, k.content);
        break;
    }
    tint(s.hurt_rgb[0], s.hurt_rgb[1], s.hurt_rgb[2], s.hurt, k.damage);
    tint(215, 186, 69, s.bonus, 1);
    switch (s.powerup) {
      case Powerup::quad:
        tint(0, 0, 255, 30, k.quad);
        break;
      case Powerup::suit:
        tint(0, 255, 0, 20, k.suit);
        break;
      case Powerup::ring:
        tint(100, 100, 100, 100, k.ring);
        break;
      case Powerup::pent:
        tint(255, 255, 0, 30, k.pent);
        break;
      case Powerup::none:
        break;
    }
    rgba[0] = c[0], rgba[1] = c[1], rgba[2] = c[2], rgba[3] = std::min(a, 1.0);
    return;
  }
  switch (s.medium) {
    case Medium::water:
      over(0.16, 0.34, 0.38, 0.35 * k.content);
      break;
    case Medium::slime:
      over(0.22, 0.40, 0.06, 0.55 * k.content);
      break;
    case Medium::lava:
      over(0.92, 0.36, 0.04, 0.65 * k.content);
      break;
    case Medium::air:
      over(s.air_rgb[0] / 255, s.air_rgb[1] / 255, s.air_rgb[2] / 255,
           std::clamp(s.air, 0.0, 255.0) / 255 * k.content);
      break;
  }
  if (s.suit) over(0.25, 0.55, 0.20, 0.18 * k.suit);
  if (s.stealth) over(0.45, 0.45, 0.45, 0.25 * k.ring);
  if (s.shield) over(0.95, 0.80, 0.25, 0.22 * k.pent);
  if (s.power) over(0.20, 0.30, 0.95, 0.22 * k.quad);
  over(0.62, 0.42, 0.12, std::min(s.armour / 300.0, 0.3) * k.damage);
  over(0.75, 0.05, 0.02, std::min(s.hurt / 300.0, 0.45) * k.damage);
  over(215 / 255.0, 186 / 255.0, 69 / 255.0, std::clamp(s.bonus, 0.0, 255.0) / 255.0 * k.bonus);
  rgba[0] = c[0], rgba[1] = c[1], rgba[2] = c[2], rgba[3] = a;
}

namespace {

// A point of the body (its offset from `d.at`, world axes) and the share at which it appears.
struct BodyPoint {
  double off[3];
  double share;
};

void turn(const float q[4], bool back, const double v[3], double out[3]) {
  const double s = back ? -1 : 1, x = s * q[0], y = s * q[1], z = s * q[2], w = q[3];
  const double t[3] = {2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]),
                       2 * (x * v[1] - y * v[0])};
  out[0] = v[0] + w * t[0] + (y * t[2] - z * t[1]);
  out[1] = v[1] + w * t[1] + (z * t[0] - x * t[2]);
  out[2] = v[2] + w * t[2] + (x * t[1] - y * t[0]);
}

BodyPoint body_point(const Dust& d, Rng& r) {
  double local[3], off[3];
  const size_t tris = d.skin.size() / 9;
  if (tris > 0) {
    // A triangle, then a point on it.
    const size_t k = std::min(tris - 1, size_t(rnd(r, 0, double(tris))));
    double a = rnd(r, 0, 1), b = rnd(r, 0, 1);
    if (a + b > 1) a = 1 - a, b = 1 - b;
    const float* t = &d.skin[k * 9];
    for (int c = 0; c < 3; ++c) local[c] = t[c] + a * (t[3 + c] - t[c]) + b * (t[6 + c] - t[c]);
    turn(d.rotation, false, local, off);
  } else {
    // Uniform over the ellipse by rejection, uniform up its height.
    double u = 0, v = 0;
    do {
      u = rnd(r, -1, 1);
      v = rnd(r, -1, 1);
    } while (u * u + v * v > 1);
    const double side[3] = {d.facing[2], 0, -d.facing[0]};
    const double h = rnd(r, d.low, d.high);
    for (int c = 0; c < 3; ++c)
      off[c] = side[c] * u * d.half_width + d.facing[c] * v * d.half_depth + (c == 1 ? h : 0);
    turn(d.rotation, true, off, local);
  }
  BodyPoint p;
  for (int c = 0; c < 3; ++c) p.off[c] = off[c];
  const float lf[3] = {float(local[0]), float(local[1]), float(local[2])};
  const float along =
      float(off[0] * d.warp.axis[0] + off[1] * d.warp.axis[1] + off[2] * d.warp.axis[2]);
  p.share = ge::render::warp_field(d.warp, lf, along) / (1 + ge::render::kWarpEdge);
  return p;
}

// A way of its own round the wind: up to `yaw` rad to either side, `lo` to `hi` rad up.
void scatter(const Dust& d, Rng& r, double yaw, double lo, double hi, double out[3]) {
  const double side[3] = {d.wind[2], 0, -d.wind[0]};
  const double a = rnd(r, -yaw, yaw), e = rnd(r, lo, hi);
  // Small angles: no trigonometry, the same bits everywhere.
  double l2 = 0;
  for (int k = 0; k < 3; ++k) {
    out[k] = d.wind[k] + side[k] * a + (k == 1 ? e : 0);
    l2 += out[k] * out[k];
  }
  const double l = std::sqrt(l2);
  for (int k = 0; k < 3; ++k) out[k] /= l;
}

// A flake of ash: near black and light, each its own size; a few still smoulder.
Particle flake(const double p[3], const double v[3], double life, Rng& r) {
  Particle q = at(p);
  for (int k = 0; k < 3; ++k) q.v[k] = float(v[k]);
  q.life = float(life);
  q.size = float(rnd(r, 0.01, 0.045));
  q.drag = 0;
  q.fall = 0;
  q.streak = 0;
  q.albedo = float(rnd(r, 0.02, 0.07));  // near black: ash, under the sky the pool lights it by
  q.opacity = float(rnd(r, 0.7, 1.0));
  // One in sixteen still smoulders a moment, so the ash reads in a dark room.
  const bool ember = rnd(r, 0, 1) < 1.0 / 16;
  q.kelvin = ember ? 1150.0f : 0.0f;
  q.cool_s = 0.35f;
  return q;
}

}  // namespace

void dust_off(const Dust& d, double from, double to, uint32_t seed, Particles* out) {
  if (!(from > to)) return;
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 1400; ++i) {
    // Every candidate draws the same numbers, kept or not, so a seed's points are the same
    // whatever the shares: the cut passing in two frames throws what it throws in one.
    const BodyPoint b = body_point(d, r);
    const double p[3] = {d.at[0] + b.off[0], d.at[1] + b.off[1], d.at[2] + b.off[2]};
    double dir[3];
    scatter(d, r, 0.9, -0.25, 0.8, dir);
    const double speed = rnd(r, 0.4, 4.5);
    double v[3];
    for (int k = 0; k < 3; ++k) v[k] = dir[k] * speed;
    Particle q = flake(p, v, rnd(r, 0.8, 2.8), r);
    q.drag = float(rnd(r, 0.15, 1.5));
    q.fall = float(rnd(r, -0.06, 0.08));            // some held up by the air, some sinking
    if (b.share < to || b.share >= from) continue;  // not crossed by the cut this frame
    out->push_back(q);
  }
}

void dust_in(const Dust& d, double was, double age, double (*land_s)(double share), uint32_t seed,
             Particles* out) {
  if (!(age > was)) return;
  Rng r = ge::core::make_rng(seed);
  const double side[3] = {d.facing[2], 0, -d.facing[0]};
  for (int i = 0; i < 1400; ++i) {
    BodyPoint b = body_point(d, r);
    if (d.keep_off_eye) {
      // Seen from inside the figure: the ash lands in the air ahead of the eyes, through the
      // whole arrival, and the eyes see it gather and go.
      const double far = rnd(r, 0.6, 1.4), across = rnd(r, -0.7, 0.7), up = rnd(r, -0.5, 0.3);
      for (int k = 0; k < 3; ++k)
        b.off[k] = d.eye[k] - d.at[k] + (d.facing[k] + side[k] * across + (k == 1 ? up : 0)) * far;
      b.share = rnd(r, 0, 0.95);
    }
    double dir[3];
    scatter(d, r, 0.9, -0.5, 0.5, dir);
    const double speed = rnd(r, 1.2, 3.5), flight = rnd(r, 0.1, 0.5);
    Rng look = r;  // the flake's own numbers, drawn whether it is born or not
    (void)rnd(r, 0, 1), (void)rnd(r, 0, 1), (void)rnd(r, 0, 1), (void)rnd(r, 0, 1);
    if (b.share >= 1) continue;
    const double land = land_s(b.share);
    // It sets off `flight` s before it lands, or at once when that is before the warp began.
    const double off = std::max(land - flight, 0.0);
    if (off >= age || (off < was && was >= 0)) continue;
    const double life = land - age;
    if (life < 0.02) continue;
    double v[3], p[3], end[3];
    for (int k = 0; k < 3; ++k) {
      v[k] = dir[k] * speed;  // downwind: from upwind of the point to it
      end[k] = d.at[k] + b.off[k];
      p[k] = end[k] - v[k] * life;  // so it ends on the point
    }
    if (d.keep_off_eye) {
      // The nearest its line comes to the eye.
      double ab[3], ae[3], ab2 = 0, t = 0;
      for (int k = 0; k < 3; ++k) {
        ab[k] = end[k] - p[k];
        ae[k] = d.eye[k] - p[k];
        ab2 += ab[k] * ab[k];
        t += ab[k] * ae[k];
      }
      t = ab2 > 0 ? std::clamp(t / ab2, 0.0, 1.0) : 0;
      double n2 = 0;
      for (int k = 0; k < 3; ++k)
        n2 += (p[k] + ab[k] * t - d.eye[k]) * (p[k] + ab[k] * t - d.eye[k]);
      if (n2 < d.eye_clear * d.eye_clear) continue;
    }
    out->push_back(flake(p, v, life, look));
  }
}

}  // namespace shooters_edge::effects
