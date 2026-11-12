// ge:summary Shooter's Edge's effects: each trail and burst's particles, the lights, the blend
// ge:keywords effects particles trail explosion speck palette ramp blood teleport lava screen-blend
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
