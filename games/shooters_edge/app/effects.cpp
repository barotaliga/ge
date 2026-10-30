// ge:summary Shooter's Edge's effects: each trail and burst's particles, the lights, the blend
// ge:keywords effects particles trail explosion spark smoke blood teleport lava screen-blend
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

double rnd(Rng& r, double lo, double hi) { return ge::core::range(r, lo, hi); }

// A direction uniform over the sphere (rejection: no trigonometry, the same bits everywhere).
void unit(Rng& r, double out[3]) {
  for (;;) {
    double l2 = 0;
    for (int k = 0; k < 3; ++k) {
      out[k] = rnd(r, -1, 1);
      l2 += out[k] * out[k];
    }
    if (l2 > 1e-4 && l2 <= 1) {
      const double l = std::sqrt(l2);
      for (int k = 0; k < 3; ++k) out[k] /= l;
      return;
    }
  }
}

Particle at(const double p[3]) {
  Particle q{};
  for (int k = 0; k < 3; ++k) q.p[k] = float(p[k]);
  return q;
}

void set_v(Particle& q, const double v[3], double speed) {
  for (int k = 0; k < 3; ++k) q.v[k] = float(v[k] * speed);
}

// A spark: hot, small, thrown at `speed` m/s along `dir`.
Particle spark(const double p[3], const double dir[3], double speed, double kelvin, double life,
               Rng& r) {
  Particle q = at(p);
  set_v(q, dir, speed);
  q.life = float(life * rnd(r, 0.7, 1.3));
  q.size = 0.025f;
  q.drag = 1.2f;
  q.fall = 0.6f;
  q.streak = 0.03f;
  q.albedo = 0.2f;
  q.opacity = 1;
  q.kelvin = float(kelvin * rnd(r, 0.9, 1.1));
  q.cool_s = float(life * 0.5);
  return q;
}

// A puff of smoke: grey, rising slowly, slowed by the air.
Particle smoke(const double p[3], double size, double grey, double opacity, double life, Rng& r) {
  Particle q = at(p);
  double d[3];
  unit(r, d);
  set_v(q, d, rnd(r, 0.1, 0.4));
  q.v[1] += float(rnd(r, 0.2, 0.5));
  q.life = float(life * rnd(r, 0.8, 1.2));
  q.size = float(size * rnd(r, 0.8, 1.3));
  q.drag = 2;
  q.fall = -0.02f;  // warm: it rises a little
  q.streak = 0;
  q.albedo = float(grey);
  q.opacity = float(opacity);
  return q;
}

void trail(const double from[3], const double to[3], uint32_t seed, bool embers, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  const int n = std::max(1, int(len / kTrailStep));
  for (int i = 0; i < n; ++i) {
    const double t = (i + rnd(r, 0, 1)) / n;
    const double p[3] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t};
    if (embers) {
      double dir[3];
      unit(r, dir);
      Particle e = spark(p, dir, rnd(r, 0.3, 1.2), 1900, 0.35, r);
      e.fall = 0.1f;
      out->push_back(e);
      out->push_back(smoke(p, 0.14, 0.32, 0.45, 0.9, r));
    } else {
      out->push_back(smoke(p, 0.11, 0.38, 0.35, 0.7, r));
    }
  }
}

}  // namespace

void rocket_trail(const double from[3], const double to[3], uint32_t seed, Particles* out) {
  trail(from, to, seed, true, out);
}

void grenade_trail(const double from[3], const double to[3], uint32_t seed, Particles* out) {
  trail(from, to, seed, false, out);
}

void explosion(const double p[3], uint32_t seed, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 140; ++i) {
    double d[3];
    unit(r, d);
    out->push_back(spark(p, d, rnd(r, 3, 11), 2300, rnd(r, 0.5, 1.1), r));
  }
  for (int i = 0; i < 36; ++i) {
    double d[3];
    unit(r, d);
    const double q[3] = {p[0] + d[0] * 0.6, p[1] + d[1] * 0.6, p[2] + d[2] * 0.6};
    out->push_back(smoke(q, 0.5, 0.18, 0.55, rnd(r, 1.2, 2.2), r));
  }
}

void impact(const double p[3], const double n[3], uint32_t seed, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 10; ++i) {
    double d[3];
    unit(r, d);
    // Off the face: the normal's half of the way, at least.
    const double along = d[0] * n[0] + d[1] * n[1] + d[2] * n[2];
    if (along < 0)
      for (int k = 0; k < 3; ++k) d[k] -= 2 * along * n[k];
    for (int k = 0; k < 3; ++k) d[k] = 0.5 * d[k] + 0.5 * n[k];
    out->push_back(spark(p, d, rnd(r, 2, 6), 1800, 0.25, r));
  }
  for (int i = 0; i < 6; ++i) {
    const double q[3] = {p[0] + n[0] * 0.05, p[1] + n[1] * 0.05, p[2] + n[2] * 0.05};
    out->push_back(smoke(q, 0.08, 0.5, 0.5, 0.6, r));
  }
}

void blood(const double p[3], const double dir[3], uint32_t seed, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 14; ++i) {
    double d[3];
    unit(r, d);
    for (int k = 0; k < 3; ++k) d[k] = 0.6 * dir[k] + 0.4 * d[k];
    Particle q = at(p);
    set_v(q, d, rnd(r, 0.8, 2.5));
    q.life = float(rnd(r, 0.5, 0.9));
    q.size = 0.03f;
    q.drag = 0.8f;
    q.fall = 1;
    q.streak = 0.02f;
    q.albedo = 0.03f;  // dark: the pool carries no colour of its own (render/particles.hpp)
    q.opacity = 0.95f;
    out->push_back(q);
  }
}

void teleport_splash(const double p[3], uint32_t seed, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 180; ++i) {
    // On a column 0.4 m round and 1.8 m tall, moving out from its axis.
    double d[3];
    unit(r, d);
    d[1] = 0;
    const double l = std::max(std::sqrt(d[0] * d[0] + d[2] * d[2]), 1e-6);
    d[0] /= l;
    d[2] /= l;
    const double q[3] = {p[0] + d[0] * 0.4, p[1] + rnd(r, 0, 1.8), p[2] + d[2] * 0.4};
    Particle s = at(q);
    set_v(s, d, rnd(r, 0.5, 1.6));
    s.life = float(rnd(r, 0.4, 0.8));
    s.size = 0.03f;
    s.drag = 2.5f;
    s.fall = 0;
    s.streak = 0.04f;
    s.albedo = 1;
    s.opacity = 1;
    s.kelvin = 7000;  // a cold white glow
    s.cool_s = 2;
    out->push_back(s);
  }
}

void lava_splash(const double p[3], uint32_t seed, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 50; ++i) {
    double d[3];
    unit(r, d);
    d[1] = std::abs(d[1]) + 1.5;
    const double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (double& c : d) c /= l;
    Particle q = spark(p, d, rnd(r, 2.5, 5.5), 1450, rnd(r, 1.0, 1.6), r);
    q.size = 0.06f;
    q.drag = 0.2f;
    q.fall = 1;
    q.cool_s = 0.8f;
    out->push_back(q);
  }
}

Light muzzle_light(const double p[3]) {
  Light l;
  for (int k = 0; k < 3; ++k) l.pos[k] = p[k];
  l.radius = 5;
  l.colour[0] = 1.6, l.colour[1] = 1.3, l.colour[2] = 0.8;
  return l;
}

Light rocket_light(const double p[3]) {
  Light l;
  for (int k = 0; k < 3; ++k) l.pos[k] = p[k];
  l.radius = 5;
  l.colour[0] = 1.2, l.colour[1] = 0.8, l.colour[2] = 0.35;
  return l;
}

bool explosion_light(const double p[3], double age, Light* out) {
  if (age < 0 || age >= kExplosionLight) return false;
  const double k = 1 - age / kExplosionLight;
  for (int i = 0; i < 3; ++i) out->pos[i] = p[i];
  out->radius = 9 * (0.5 + 0.5 * k);
  out->colour[0] = 2.2 * k, out->colour[1] = 1.4 * k, out->colour[2] = 0.6 * k;
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
    case Medium::water: over(0.16, 0.34, 0.38, 0.35); break;
    case Medium::slime: over(0.22, 0.40, 0.06, 0.55); break;
    case Medium::lava: over(0.92, 0.36, 0.04, 0.65); break;
    case Medium::air: break;
  }
  if (s.suit) over(0.25, 0.55, 0.20, 0.18);
  if (s.stealth) over(0.45, 0.45, 0.45, 0.25);
  if (s.shield) over(0.95, 0.80, 0.25, 0.22);
  if (s.power) over(0.20, 0.30, 0.95, 0.22);
  over(0.75, 0.05, 0.02, std::min(s.hurt / 150.0, 0.6));
  rgba[0] = c[0], rgba[1] = c[1], rgba[2] = c[2], rgba[3] = a;
}

}  // namespace shooters_edge::effects
