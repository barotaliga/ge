// ge:summary Shooter's Edge's effects: each trail and burst's particles, the lights, the blend
// ge:keywords effects particles trail explosion spark smoke blood teleport lava screen-blend
// ge:keywords dust dust_off dust_in ash flakes warp_field
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
      Particle e = spark(p, dir, rnd(r, 0.3, 1.2), 1400, 0.35, r);
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
    out->push_back(spark(p, d, rnd(r, 3, 11), 1650, rnd(r, 0.5, 1.1), r));
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
    out->push_back(spark(p, d, rnd(r, 2, 6), 1500, 0.25, r));
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
    s.kelvin = 1700;  // a glow that dulls as it spreads
    s.cool_s = 0.4f;
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
    Particle q = spark(p, d, rnd(r, 2.5, 5.5), 1300, rnd(r, 1.0, 1.6), r);
    q.size = 0.06f;
    q.drag = 0.2f;
    q.fall = 1;
    q.cool_s = 0.8f;
    out->push_back(q);
  }
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
