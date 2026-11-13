// The monsters' attacks in the modern look (monster_fx.hpp).
//
// ge:summary The monsters' missiles and spit in the modern look: acid, flame, void, lava, laser
// ge:keywords monster-fx acid flame void-ball lava-ball laser flesh tracer splash
// ge:skill render shooter
// ge:test tests/shooters_edge_monster_fx_test.cpp
#include "shooters_edge/app/monster_fx.hpp"

#include <algorithm>
#include <cmath>

#include "core/prng.hpp"
#include "shooters_edge/app/weapon_fx.hpp"

namespace shooters_edge::effects {

namespace gr = ge::render;

namespace {

using ge::core::Rng;
using gr::Particle;

constexpr double kUnit = 0.025;  // a map unit, m
constexpr double kTau = 6.283185307179586;
constexpr float kClassicFall = float(40 * kUnit / gr::kGravity);

double rnd(Rng& r, double lo, double hi) { return ge::core::range(r, lo, hi); }
int pick(Rng& r, int n) { return std::min(int(rnd(r, 0, n)), n - 1); }

// A direction uniform over the sphere (rejection: the same bits everywhere).
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

void away_from(const double n[3], double d[3]) {
  const double along = d[0] * n[0] + d[1] * n[1] + d[2] * n[2];
  if (along < 0)
    for (int k = 0; k < 3; ++k) d[k] -= 2 * along * n[k];
}

// Two unit axes square to `dir` (any, when it is zero).
void across(const double dir[3], double a[3], double b[3]) {
  const double up[3] = {0, 1, 0}, side[3] = {1, 0, 0};
  const double* ref = std::abs(dir[1]) < 0.9 ? up : side;
  a[0] = dir[1] * ref[2] - dir[2] * ref[1];
  a[1] = dir[2] * ref[0] - dir[0] * ref[2];
  a[2] = dir[0] * ref[1] - dir[1] * ref[0];
  double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  if (l < 1e-9) {
    a[0] = 1, a[1] = 0, a[2] = 0;
    l = 1;
  }
  for (int k = 0; k < 3; ++k) a[k] /= l;
  b[0] = dir[1] * a[2] - dir[2] * a[1];
  b[1] = dir[2] * a[0] - dir[0] * a[2];
  b[2] = dir[0] * a[1] - dir[1] * a[0];
}

Particle at(const double p[3]) {
  Particle q{};
  for (int k = 0; k < 3; ++k) q.p[k] = float(p[k]);
  q.opacity = 1;
  return q;
}

// A speck that shines its own colour (`rgb`, linear) `level` times the white the picture is keyed
// to, its colour going toward a grey of `fade_to` by e every `cool` s.
Particle shine(const double p[3], const float rgb[3], double level, double fade_to, double cool,
               double life, double size) {
  Particle q = at(p);
  for (int k = 0; k < 3; ++k) q.tint[k] = rgb[k];
  q.kelvin = float(-level);
  q.albedo = float(fade_to);
  q.cool_s = float(cool);
  q.life = float(life);
  q.size = float(size);
  return q;
}

// A glowing ember: `kelvin` at birth, cooling by e every `cool` s.
Particle ember(const double p[3], const double d[3], double speed, double kelvin, double cool,
               double life, Rng& r) {
  Particle q = at(p);
  for (int k = 0; k < 3; ++k) q.v[k] = float(d[k] * speed);
  q.life = float(life);
  q.size = float(rnd(r, 0.03, 0.05));
  q.drag = 1.0f;
  q.fall = 0.6f;
  q.streak = 0.025f;
  q.albedo = 0.05f;
  q.kelvin = float(kelvin * rnd(r, 0.92, 1.08));
  q.cool_s = float(cool);
  return q;
}

// A puff that spreads as it thins: smoke (lit, `grey`), or with `glow` > 0 a shining haze.
Particle puff(const double p[3], double size, double grow, double grey, double opacity, double life,
              const float tint[3], Rng& r) {
  Particle q = at(p);
  double d[3];
  unit(r, d);
  for (int k = 0; k < 3; ++k) q.v[k] = float(d[k] * rnd(r, 0.05, 0.25));
  q.life = float(life * rnd(r, 0.8, 1.2));
  q.size = float(size * rnd(r, 0.8, 1.25));
  q.grow = float(grow * rnd(r, 0.8, 1.2));
  q.drag = 2.5f;
  q.fall = -0.03f;
  q.albedo = float(grey);
  q.opacity = float(opacity);
  for (int k = 0; k < 3; ++k) q.tint[k] = tint[k];
  return q;
}

// The kinds' colours (linear).
constexpr float kAcid[3] = {0.45f, 1.0f, 0.12f};
constexpr float kAcidHot[3] = {0.85f, 1.0f, 0.35f};
constexpr float kAcidMist[3] = {0.55f, 0.9f, 0.4f};
constexpr float kVoid[3] = {0.62f, 0.25f, 1.0f};
constexpr float kVoidHot[3] = {0.95f, 0.8f, 1.0f};
constexpr float kVoidSmoke[3] = {0.55f, 0.4f, 0.75f};
constexpr float kLaser[3] = {1.0f, 0.32f, 0.08f};
constexpr float kLaserHot[3] = {1.0f, 0.85f, 0.55f};
constexpr float kBlood[3] = {0.55f, 0.03f, 0.02f};
constexpr float kSmoke[3] = {1.0f, 0.93f, 0.86f};
constexpr float kDarkSmoke[3] = {0.9f, 0.85f, 0.8f};

// The stretch's points every `step` m, each at a random place within its step.
template <class F>
void along(const double from[3], const double to[3], double step, Rng& r, F&& f) {
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  const int n = std::max(1, int(len / step));
  double dir[3] = {0, 0, 0};
  if (len > 1e-6)
    for (int k = 0; k < 3; ++k) dir[k] = d[k] / len;
  for (int i = 0; i < n; ++i) {
    const double t = (i + rnd(r, 0, 1)) / n;
    const double p[3] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t};
    f(p, dir);
  }
}

gr::MuzzleLook make_flame_bolt(bool lava) {
  gr::MuzzleLook l = *gr::muzzle_look_by_id("muzzle.rocket");
  l.id = lava ? "se.monster.lava" : "se.monster.flame";
  l.length_m = lava ? 0.7 : 0.45;
  l.width_m = lava ? 0.32 : 0.16;
  l.life_s = 0.06;
  l.core_k = lava ? 2100 : 2300;
  l.cool_k = lava ? 1300 : 1500;
  l.glow = lava ? 0.18 : 0.05;
  l.flash = 0;
  l.billows = 1.0;
  l.fray = 0.6;
  l.smoke.smoke = 0;
  l.sparks = 0;
  return l;
}

gr::BlastLook make_flame_burst() {
  gr::BlastLook l = blast_look();
  l.id = "se.monster.flame.burst";
  l.radius_m = 0.45;
  l.grow_s = 0.03;
  l.fire_s = 0.18;
  l.core_k = 2300;
  l.cool_s = 0.2;
  l.flash = 0.35;
  l.smoke = 0.35;
  l.smoke_s = 1.1;
  l.debris = 0;
  l.light_range_m = 0;
  l.scorch_m = 0;
  return l;
}

const gr::MuzzleLook& flame_bolt_look(bool lava) {
  static const gr::MuzzleLook flame = make_flame_bolt(false), molten = make_flame_bolt(true);
  return lava ? molten : flame;
}

}  // namespace

const gr::BlastLook& flame_burst_look() {
  static const gr::BlastLook l = make_flame_burst();
  return l;
}

MonsterShot monster_shot_of(std::string_view e) {
  if (e == "missile.acid") return MonsterShot::acid;
  if (e == "missile.flame") return MonsterShot::flame;
  if (e == "missile.void") return MonsterShot::void_ball;
  if (e == "missile.lava") return MonsterShot::lava;
  if (e == "missile.laser") return MonsterShot::laser;
  if (e == "missile.flesh") return MonsterShot::flesh;
  return MonsterShot::none;
}

MonsterShot monster_shot_of_flags(uint32_t f) {
  if ((f & kTracerGreen) != 0) return MonsterShot::acid;
  if ((f & kTracerOrange) != 0) return MonsterShot::flame;
  if ((f & kTracerPurple) != 0) return MonsterShot::void_ball;
  return MonsterShot::none;
}

const char* monster_shot_name(MonsterShot k) {
  switch (k) {
    case MonsterShot::acid:
      return "acid";
    case MonsterShot::flame:
      return "flame";
    case MonsterShot::void_ball:
      return "void";
    case MonsterShot::lava:
      return "lava";
    case MonsterShot::laser:
      return "laser";
    case MonsterShot::flesh:
      return "flesh";
    case MonsterShot::none:
      break;
  }
  return "none";
}

int classic_tracer(MonsterShot k, const double from[3], const double to[3], int count,
                   uint32_t seed, Palette pal, Particles* out) {
  if (k == MonsterShot::lava) {
    rocket_trail(from, to, seed, pal, out);
    return count;
  }
  if (k == MonsterShot::laser || k == MonsterShot::none) return count;
  Rng r = ge::core::make_rng(seed);
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  const double step = (k == MonsterShot::flesh ? 6 : 3) * kUnit;
  const int n = int(len / step);
  if (len < 1e-6) return count;
  for (int i = 0; i < n; ++i) {
    const double t = (i + 0.5) / std::max(n, 1);
    const double p[3] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t};
    Particle s = at(p);
    s.kelvin = -1;
    s.size = 0.05f;
    s.life = 0.5f;
    int index = 0;
    switch (k) {
      case MonsterShot::acid:
        index = 52 + ((count & 4) << 1);
        break;
      case MonsterShot::flame:
        index = 230 + ((count & 4) << 1);
        break;
      case MonsterShot::void_ball:
        index = 152 + pick(r, 4);
        s.life = 0.3f;
        for (float& c : s.p) c += float(rnd(r, -8, 8) * kUnit);
        break;
      case MonsterShot::flesh:
        index = 67 + pick(r, 4);
        s.life = 2;
        s.fall = kClassicFall;
        break;
      default:
        break;
    }
    palette_rgb(pal, index, s.tint);
    s.albedo = 0.2126f * s.tint[0] + 0.7152f * s.tint[1] + 0.0722f * s.tint[2];
    if (k == MonsterShot::acid || k == MonsterShot::flame) {
      // Thrown either side of the line in turn, slowly.
      const double side = (count & 1) != 0 ? 1 : -1;
      const double v[3] = {d[2] / len * 30 * kUnit * side, 0, -d[0] / len * 30 * kUnit * side};
      for (int c = 0; c < 3; ++c) s.v[c] = float(v[c]);
    }
    ++count;
    out->push_back(s);
  }
  return count;
}

void classic_spit_hit(MonsterShot k, const double p[3], uint32_t seed, Palette pal,
                      Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const bool acid = k == MonsterShot::acid;
  const int n = acid ? 30 : 20;
  for (int i = 0; i < n; ++i) {
    double q[3];
    for (int c = 0; c < 3; ++c) q[c] = p[c] + rnd(r, -8, 8) * kUnit;
    Particle s = at(q);
    palette_rgb(pal, (acid ? 20 : 226) + pick(r, 8), s.tint);
    s.albedo = 0.2126f * s.tint[0] + 0.7152f * s.tint[1] + 0.0722f * s.tint[2];
    s.kelvin = -1;
    s.size = 0.05f;
    s.fall = kClassicFall;
    s.life = float(0.1 * pick(r, 5) + 0.02);
    out->push_back(s);
  }
}

void monster_trail(MonsterShot k, const double from[3], const double to[3], uint32_t seed,
                   Particles* out) {
  Rng r = ge::core::make_rng(seed);
  switch (k) {
    case MonsterShot::acid:
      along(from, to, 0.07, r, [&](const double p[3], const double dir[3]) {
        // Drops of acid that drip off and fall, glowing green, in a thin green mist.
        Particle d = shine(p, kAcid, 1.7, 0.25, 0.6, rnd(r, 0.35, 0.7), rnd(r, 0.04, 0.08));
        double v[3];
        unit(r, v);
        for (int c = 0; c < 3; ++c) d.v[c] = float(v[c] * rnd(r, 0.1, 0.5) - dir[c] * 0.4);
        d.fall = 1.0f;
        d.streak = 0.03f;
        d.drag = 0.6f;
        out->push_back(d);
        if (rnd(r, 0, 1) < 0.4) {
          out->push_back(puff(p, rnd(r, 0.06, 0.12), rnd(r, 0.25, 0.5), 0.5, rnd(r, 0.1, 0.22),
                              rnd(r, 0.5, 0.9), kAcidMist, r));
        }
      });
      break;
    case MonsterShot::flame:
      along(from, to, 0.07, r, [&](const double p[3], const double dir[3]) {
        double v[3];
        unit(r, v);
        for (int c = 0; c < 3; ++c) v[c] = v[c] * 0.6 - dir[c] * 0.4;
        Particle e = ember(p, v, rnd(r, 0.3, 1.2), 2200, 0.09, rnd(r, 0.2, 0.45), r);
        e.fall = -0.15f;
        e.streak = 0.0f;
        e.size = float(rnd(r, 0.04, 0.08));
        out->push_back(e);
        out->push_back(puff(p, rnd(r, 0.05, 0.1), rnd(r, 0.2, 0.5), 0.25, rnd(r, 0.1, 0.3),
                            rnd(r, 0.6, 1.2), kSmoke, r));
      });
      break;
    case MonsterShot::void_ball: {
      double a[3], c[3];
      along(from, to, 0.05, r, [&](const double p[3], const double dir[3]) {
        across(dir, a, c);
        // Motes spiralling off round the path, and a dark wake.
        const double ang = rnd(r, 0, kTau), rad = rnd(r, 0.08, 0.2);
        const double ca = std::cos(ang), sa = std::sin(ang);
        double q[3], v[3];
        for (int k3 = 0; k3 < 3; ++k3) {
          q[k3] = p[k3] + (a[k3] * ca + c[k3] * sa) * rad;
          v[k3] = (-a[k3] * sa + c[k3] * ca) * 1.2 + (a[k3] * ca + c[k3] * sa) * 0.4;
        }
        Particle m = shine(q, kVoid, 2.2, 0.15, 0.25, rnd(r, 0.3, 0.6), rnd(r, 0.035, 0.065));
        for (int k3 = 0; k3 < 3; ++k3) m.v[k3] = float(v[k3]);
        m.drag = 1.5f;
        m.fall = 0;
        m.swirl = 0.2f;
        out->push_back(m);
        if (rnd(r, 0, 1) < 0.5) {
          out->push_back(puff(p, rnd(r, 0.08, 0.14), rnd(r, 0.3, 0.6), 0.08, rnd(r, 0.2, 0.4),
                              rnd(r, 0.6, 1.0), kVoidSmoke, r));
        }
      });
      break;
    }
    case MonsterShot::lava:
      along(from, to, 0.08, r, [&](const double p[3], const double dir[3]) {
        double v[3];
        unit(r, v);
        // The rocket's fire and smoke, heavier; molten drops falling off it.
        Particle e = ember(p, v, rnd(r, 0.3, 1.0), 2000, 0.15, rnd(r, 0.3, 0.6), r);
        e.size = float(rnd(r, 0.07, 0.12));
        e.fall = -0.1f;
        e.streak = 0;
        out->push_back(e);
        Particle s = puff(p, rnd(r, 0.12, 0.25), rnd(r, 0.4, 1.0), 0.25, rnd(r, 0.25, 0.55),
                          rnd(r, 1.2, 2.4), kSmoke, r);
        out->push_back(s);
        if (rnd(r, 0, 1) < 0.5) {
          for (int c = 0; c < 3; ++c) v[c] = v[c] * 0.3 - dir[c] * 0.2;
          Particle d = ember(p, v, rnd(r, 0.5, 1.5), 1700, 0.8, rnd(r, 0.8, 1.6), r);
          d.fall = 1;
          d.size = float(rnd(r, 0.03, 0.06));
          d.streak = 0.03f;
          d.bounce = 0.2f;
          out->push_back(d);
        }
      });
      break;
    case MonsterShot::laser:
      along(from, to, 0.1, r, [&](const double p[3], const double dir[3]) {
        // A glowing streak along its line, fading fast.
        Particle m = shine(p, kLaser, 2.4, 0.1, 0.05, rnd(r, 0.06, 0.12), 0.06);
        for (int c = 0; c < 3; ++c) m.v[c] = float(dir[c] * 2);
        m.streak = 0.04f;
        out->push_back(m);
      });
      break;
    case MonsterShot::flesh:
      along(from, to, 0.12, r, [&](const double p[3], const double dir[3]) {
        double v[3];
        unit(r, v);
        Particle d = at(p);
        for (int c = 0; c < 3; ++c) {
          d.v[c] = float(v[c] * rnd(r, 0.1, 0.4) + dir[c] * 0.3);
          d.tint[c] = kBlood[c] * 1.8f;
        }
        d.albedo = 0.35f;
        d.life = float(rnd(r, 1.0, 2.0));
        d.size = float(rnd(r, 0.02, 0.045));
        d.fall = 1;
        d.streak = 0.03f;
        d.floor_y = -1e9f;
        out->push_back(d);
      });
      break;
    case MonsterShot::none:
      break;
  }
}

void monster_head(MonsterShot k, const double p[3], const double dir[3], double speed,
                  Particles* out) {
  const float* core = nullptr;
  double core_m = 0.12, level = 3;
  constexpr float kFireHot[3] = {1.0f, 0.8f, 0.45f};
  switch (k) {
    case MonsterShot::acid:
      core = kAcidHot;
      break;
    case MonsterShot::flame:
      core = kFireHot;
      core_m = 0.08;  // a small hot point: the flame round it is the bolt, not a glowing bulb
      level = 2;
      break;
    case MonsterShot::void_ball:
      core = kVoidHot;
      core_m = 0.16;
      level = 3.5;
      break;
    case MonsterShot::lava:
      core = kFireHot;
      core_m = 0.25;
      break;
    case MonsterShot::laser:
      core = kLaserHot;
      core_m = 0.06;
      level = 4;
      break;
    default:
      return;
  }
  // A white-hot core streaked along its flight (a laser's to a line); the bloom and its light
  // make the glow round it.
  Particle c = shine(p, core, level, 0.5, 0, 0.035, core_m);
  for (int i = 0; i < 3; ++i) c.v[i] = float(dir[i] * speed);
  c.streak = k == MonsterShot::laser ? 0.035f : 0.012f;
  c.fall = 0;
  c.opacity = 0.9f;
  out->push_back(c);
}

bool monster_glow(MonsterShot k, const double p[3], const double dir[3], uint32_t seed,
                  double now_s, Light* out) {
  const double phase = static_cast<double>(seed % 997u) * 0.37;
  const double waver = 0.5 + 0.3 * std::sin(now_s * kTau * 9.1 + phase) +
                       0.2 * std::sin(now_s * kTau * 13.7 + phase * 1.7);
  const float* rgb = nullptr;
  double level = 1, reach = 3;
  switch (k) {
    case MonsterShot::acid:
      rgb = kAcid;
      level = 0.9;
      reach = 3;
      break;
    case MonsterShot::flame:
      rgb = nullptr;
      level = 0.8;
      reach = 4.5;
      break;
    case MonsterShot::void_ball:
      rgb = kVoid;
      level = 1.2;
      reach = 4.5;
      break;
    case MonsterShot::lava:
      level = 2.2;
      reach = 7.5;
      break;
    case MonsterShot::laser:
      rgb = kLaser;
      level = 1.4;
      reach = 3.5;
      break;
    default:
      return false;
  }
  for (int c = 0; c < 3; ++c) out->pos[c] = p[c] - dir[c] * 0.1;
  out->radius = reach;
  constexpr double kFlameRgb[3] = {1.0, 0.6, 0.28};
  const double flick = k == MonsterShot::laser ? 1 : 0.8 + 0.2 * waver;
  for (int c = 0; c < 3; ++c)
    out->colour[c] = level * flick * (rgb != nullptr ? double(rgb[c]) : kFlameRgb[c]);
  return true;
}

bool monster_flame(MonsterShot k, const double p[3], const double dir[3], uint32_t seed,
                   double now_s, gr::MuzzleFire* out) {
  const bool lava = k == MonsterShot::lava;
  if (k != MonsterShot::flame && !lava) return false;
  const double l = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  if (l < 1e-6) return false;
  gr::MuzzleFire f;
  f.shot.look = &flame_bolt_look(lava);
  f.shot.seed = seed;
  f.shot.born_s = now_s - 0.012;
  double a[3], b[3];
  const double d[3] = {dir[0] / l, dir[1] / l, dir[2] / l};
  across(d, a, b);
  for (int c = 0; c < 3; ++c) {
    f.pose.at[c] = p[c] + d[c] * (lava ? 0.2 : 0.1);
    f.pose.dir[c] = -d[c];
    f.pose.up[c] = a[c];
  }
  *out = f;
  return true;
}

gr::MuzzleFire monster_muzzle(const double p[3], const double dir[3], uint32_t seed,
                              double born_s) {
  gr::MuzzleFire f;
  f.shot.look = gr::muzzle_look_by_id("muzzle.shotgun");
  f.shot.seed = seed;
  f.shot.born_s = born_s;
  double a[3], b[3];
  across(dir, a, b);
  for (int c = 0; c < 3; ++c) {
    f.pose.at[c] = p[c];
    f.pose.dir[c] = dir[c];
    f.pose.up[c] = -b[c];  // the world's up, square to the barrel
  }
  return f;
}

void monster_splash(MonsterShot k, const double p[3], const double* normal, const double dir[3],
                    double floor_y, uint32_t seed, double now_s, std::vector<gr::Blast>* blasts,
                    Particles* births) {
  Rng r = ge::core::make_rng(seed);
  double n[3] = {-dir[0], -dir[1], -dir[2]};
  if (normal != nullptr)
    for (int c = 0; c < 3; ++c) n[c] = normal[c];
  const double off[3] = {p[0] + n[0] * 0.05, p[1] + n[1] * 0.05, p[2] + n[2] * 0.05};
  switch (k) {
    case MonsterShot::acid: {
      // Droplets thrown off the face that fall and run, a hiss of green haze.
      for (int i = 0; i < 36; ++i) {
        double d[3];
        unit(r, d);
        away_from(n, d);
        Particle q = shine(off, i % 3 == 0 ? kAcidHot : kAcid, 1.8, 0.2, 0.5, rnd(r, 0.4, 0.9),
                           rnd(r, 0.035, 0.07));
        for (int c = 0; c < 3; ++c) q.v[c] = float(d[c] * rnd(r, 0.6, 2.5));
        q.fall = 1;
        q.drag = 0.8f;
        q.streak = 0.03f;
        q.floor_y = float(floor_y);
        q.bounce = 0.1f;
        births->push_back(q);
      }
      for (int i = 0; i < 6; ++i)
        births->push_back(puff(off, rnd(r, 0.1, 0.2), rnd(r, 0.4, 0.8), 0.55, rnd(r, 0.15, 0.3),
                               rnd(r, 0.8, 1.5), kAcidMist, r));
      break;
    }
    case MonsterShot::flame: {
      if (blasts != nullptr) {
        gr::Blast f;
        for (int c = 0; c < 3; ++c) f.at[c] = p[c];
        if (normal != nullptr) {
          f.on_surface = true;
          for (int c = 0; c < 3; ++c) f.normal[c] = normal[c];
        }
        f.look = &flame_burst_look();
        f.seed = seed;
        f.born_s = now_s;
        blasts->push_back(f);
      }
      for (int i = 0; i < 30; ++i) {
        double d[3];
        unit(r, d);
        away_from(n, d);
        Particle e = ember(off, d, rnd(r, 1, 4), 2300, 0.25, rnd(r, 0.3, 0.8), r);
        e.floor_y = float(floor_y);
        e.bounce = 0.3f;
        births->push_back(e);
      }
      break;
    }
    case MonsterShot::laser:
      for (int i = 0; i < 14; ++i) {
        double d[3];
        unit(r, d);
        away_from(n, d);
        Particle s = shine(off, kLaserHot, 2.5, 0.2, 0.1, rnd(r, 0.1, 0.35), 0.015);
        for (int c = 0; c < 3; ++c) s.v[c] = float(d[c] * rnd(r, 2, 6));
        s.streak = 0.03f;
        s.fall = 0.6f;
        s.floor_y = float(floor_y);
        s.bounce = 0.3f;
        births->push_back(s);
      }
      break;
    default:
      break;
  }
}

bool splash_light(MonsterShot k, const double p[3], double age, Light* out) {
  if (age < 0 || age >= kSplashLightS) return false;
  const double left = 1 - age / kSplashLightS;
  const float* rgb = nullptr;
  double level = 1.2, reach = 3;
  constexpr float kFire[3] = {1.0f, 0.6f, 0.28f};
  switch (k) {
    case MonsterShot::acid:
      rgb = kAcid;
      break;
    case MonsterShot::flame:
      rgb = kFire;
      level = 2.0;
      reach = 4.5;
      break;
    case MonsterShot::laser:
      rgb = kLaser;
      level = 1.0;
      reach = 2;
      break;
    default:
      return false;
  }
  for (int c = 0; c < 3; ++c) out->pos[c] = p[c];
  out->radius = reach;
  for (int c = 0; c < 3; ++c) out->colour[c] = level * left * left * rgb[c];
  return true;
}

}  // namespace shooters_edge::effects
