// ge:summary Shooter's Edge's effects: each trail and burst's particles, the lights, the blend
// ge:keywords effects particles trail explosion speck palette ramp blood teleport lava screen-blend
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/effects.hpp"

#include <algorithm>
#include <cmath>

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

void impact(const double p[3], int specks, uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double spread = specks > 130 ? 24 : specks > 20 ? 16 : 8;
  for (int i = 0; i < specks; ++i) {
    double q[3];
    jitter(r, p, spread, q);
    out->push_back(speck(q, pal, pick(r, 8), 0, 0, 0.1 * pick(r, 5) + 0.02));
  }
}

void blood(const double p[3], int specks, bool arc, uint32_t seed, Palette pal, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < specks; ++i) {
    double q[3];
    jitter(r, p, 8, q);
    out->push_back(speck(q, pal, (arc ? 224 : 72) + pick(r, 8), 0, 0, 0.1 * pick(r, 5) + 0.02));
  }
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

Light rocket_light(const double p[3]) { return classic_light(p, 200); }

bool explosion_light(const double p[3], double age, Light* out) {
  if (age < 0 || age >= kExplosionLight) return false;
  *out = classic_light(p, 350 - 300 * age);  // 350 units, shrinking 300 a second
  return true;
}

double fade_hurt(double hurt, double dt) { return std::max(hurt - kHurtFade * dt, 0.0); }

double take_hurt(double hurt, double damage) { return std::min(hurt + 3 * damage, 150.0); }

void screen_blend(const Screen& s, double rgba[4]) {
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
  switch (s.medium) {
    case Medium::water:
      over(0.16, 0.34, 0.38, 0.35);
      break;
    case Medium::slime:
      over(0.22, 0.40, 0.06, 0.55);
      break;
    case Medium::lava:
      over(0.92, 0.36, 0.04, 0.65);
      break;
    case Medium::air:
      break;
  }
  if (s.suit) over(0.25, 0.55, 0.20, 0.18);
  if (s.stealth) over(0.45, 0.45, 0.45, 0.25);
  if (s.shield) over(0.95, 0.80, 0.25, 0.22);
  if (s.power) over(0.20, 0.30, 0.95, 0.22);
  over(0.62, 0.42, 0.12, std::min(s.armour / 300.0, 0.3));
  over(0.75, 0.05, 0.02, std::min(s.hurt / 300.0, 0.45));
  rgba[0] = c[0], rgba[1] = c[1], rgba[2] = c[2], rgba[3] = a;
}

}  // namespace shooters_edge::effects
