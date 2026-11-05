// Shooter's Edge's enhanced weapon effects (weapon_fx.hpp).
//
// ge:summary Shooter's Edge's enhanced weapon effects: layered blasts, exhaust, the bolt, the hits
// ge:keywords weapon-fx blast billows embers exhaust rocket-trail grenade-trail bolt hit
// ge:skill render shooter
// ge:test tests/shooters_edge_weapon_fx_test.cpp
#include "shooters_edge/app/weapon_fx.hpp"

#include <algorithm>
#include <cmath>

#include "core/prng.hpp"
#include "render/impact.hpp"

namespace shooters_edge::effects {

namespace {

using ge::core::Rng;
using gr::Particle;

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

// `d` turned into the half space `n` faces (mirrored off the surface when it points into it).
void away_from(const double n[3], double d[3]) {
  const double along = d[0] * n[0] + d[1] * n[1] + d[2] * n[2];
  if (along < 0)
    for (int k = 0; k < 3; ++k) d[k] -= 2 * along * n[k];
}

Particle at(const double p[3]) {
  Particle q{};
  for (int k = 0; k < 3; ++k) q.p[k] = float(p[k]);
  return q;
}

void set_v(Particle& q, const double d[3], double speed) {
  for (int k = 0; k < 3; ++k) q.v[k] = float(d[k] * speed);
}

// A glowing ember: `kelvin` at birth, cooling by e every `cool` s.
Particle ember(const double p[3], const double d[3], double speed, double kelvin, double cool,
               double life, Rng& r) {
  Particle q = at(p);
  set_v(q, d, speed);
  q.life = float(life);
  q.size = float(rnd(r, 0.03, 0.05));
  q.drag = 1.0f;
  q.fall = 0.6f;
  q.streak = 0.025f;
  q.albedo = 0.05f;
  q.opacity = 1;
  q.kelvin = float(kelvin * rnd(r, 0.92, 1.08));
  q.cool_s = float(cool);
  return q;
}

// A puff of smoke that rises a little and spreads as it thins.
Particle puff(const double p[3], double size, double grow, double grey, double opacity, double life,
              const float tint[3], Rng& r) {
  Particle q = at(p);
  double d[3];
  unit(r, d);
  set_v(q, d, rnd(r, 0.05, 0.25));
  q.v[1] += float(rnd(r, 0.15, 0.4));
  q.life = float(life * rnd(r, 0.8, 1.2));
  q.size = float(size * rnd(r, 0.8, 1.25));
  q.grow = float(grow * rnd(r, 0.8, 1.2));
  q.drag = 2.5f;
  q.fall = -0.03f;  // warm: it rises
  q.albedo = float(grey);
  q.opacity = float(opacity);
  for (int k = 0; k < 3; ++k) q.tint[k] = tint[k];
  return q;
}

gr::BlastLook make_blast() {
  gr::BlastLook l = *gr::blast_look_by_id("explosion.rocket");
  l.id = "se.blast";
  l.radius_m = 1.35;  // the classic sprite's ball, about 1.6 m across at its widest
  l.grow_s = 0.05;
  l.smoke_grow = 0.55;
  l.rise = 1.1;
  l.fire_s = 0.3;
  l.core_k = 2450;  // orange-white at its hottest, not a white bulb
  l.cool_k = 1400;
  l.cool_s = 0.3;
  l.flash = 0.55;
  l.smoke = 0.75;
  l.smoke_in_s = 0.25;
  l.smoke_s = 2.4;
  l.smoke_albedo = 0.14;
  l.smoke_tint[0] = 0.95f, l.smoke_tint[1] = 0.86f, l.smoke_tint[2] = 0.74f;
  l.billows = 0.95;
  l.fray = 0.4;
  l.billow_size = 1.1;
  l.roll = 0.7;
  l.sparks = 0;  // thrown by `blast`, the classic's way
  l.debris = 6;
  l.debris_hot = 0.6;
  l.light_range_m = 10;
  l.scorch_m = 3.0;  // the mark's box: its dark heart about half of it, read across a room
  return l;
}

gr::BlastLook make_billow() {
  gr::BlastLook l = make_blast();
  l.id = "se.blast.billow";
  l.radius_m = 0.7;
  l.grow_s = 0.03;
  l.fire_s = 0.22;
  l.core_k = 2450;
  l.cool_s = 0.25;
  l.flash = 0.3;
  l.smoke = 0.6;
  l.smoke_s = 1.9;
  l.billows = 1.1;
  l.fray = 0.5;
  l.debris = 0;
  l.light_range_m = 0;
  return l;
}

gr::MuzzleLook make_exhaust() {
  gr::MuzzleLook l = *gr::muzzle_look_by_id("muzzle.rocket");
  l.id = "se.exhaust";
  l.length_m = 0.5;
  l.width_m = 0.13;
  l.life_s = 0.05;
  l.core_k = 2400;
  l.cool_k = 1700;
  l.glow = 0.12;
  l.flash = 0;
  l.billows = 0.8;
  l.fray = 0.5;
  l.smoke.smoke = 0;  // the trail's puffs are its smoke
  l.sparks = 0;
  return l;
}

// The light any style throws on the room: a cold glow along it and where it lands.
void cold_light(gr::BeamLook& l) {
  l.light_rgb[0] = 0.55f, l.light_rgb[1] = 0.65f, l.light_rgb[2] = 1.0f;
  l.light_cd = 900;
  l.hit_cd = 700;
  l.root_cd = 150;
}

gr::BeamLook make_forked() {
  gr::BeamLook l;
  gr::beam_look("bolt", l);
  l.step_m = 0.3f;  // kinked every 30 cm, wandering far off its line over a few metres
  l.wander = 0.55f;
  l.pull = 0.18f;
  l.fork_chance = 0.07f;
  l.fork_m = 0.5f;
  l.strike_hz = 20;
  l.core_m = 0.06f;
  l.glow_m = 0.55f;
  l.core_cd = 9e4f;
  l.glow_cd = 7000;  // the ionised air's blue round it, bright enough to read by day
  l.core_rgb[0] = 0.86f, l.core_rgb[1] = 0.9f, l.core_rgb[2] = 1.0f;
  l.glow_rgb[0] = 0.38f, l.glow_rgb[1] = 0.5f, l.glow_rgb[2] = 1.0f;
  l.flash = 0.8f;
  l.flicker = 0.5f;
  l.shimmer = 0.35f;
  l.attack_s = 0.02f;
  l.release_s = 0.06f;
  cold_light(l);
  return l;
}

// One of the engine's presets as it makes it, its light turned down.
gr::BeamLook make_preset(std::string_view name) {
  gr::BeamLook l;
  gr::beam_look(name, l);
  cold_light(l);
  return l;
}

constexpr float kWarmGrey[3] = {1.0f, 0.95f, 0.88f};
constexpr float kPaleGrey[3] = {0.96f, 0.96f, 0.98f};

}  // namespace

const gr::BlastLook& blast_look() {
  static const gr::BlastLook l = make_blast();
  return l;
}
const gr::BlastLook& billow_look() {
  static const gr::BlastLook l = make_billow();
  return l;
}
const gr::MuzzleLook& exhaust_look() {
  static const gr::MuzzleLook l = make_exhaust();
  return l;
}
const gr::BeamLook& bolt_look(int style) {
  static const gr::BeamLook kLooks[] = {make_forked(), make_preset(kBoltStyles[1]),
                                        make_preset(kBoltStyles[2]), make_preset(kBoltStyles[3])};
  const int n = static_cast<int>(std::size(kLooks));
  return kLooks[(style >= 1 && style <= n ? style : kDefaultBoltStyle) - 1];
}

void blast(const double p[3], const double* normal, uint32_t seed, double now_s,
           std::vector<gr::Blast>* blasts, Particles* births) {
  Rng r = ge::core::make_rng(seed);
  gr::Blast main;
  for (int k = 0; k < 3; ++k) main.at[k] = p[k];
  if (normal != nullptr) {
    main.on_surface = true;
    for (int k = 0; k < 3; ++k) main.normal[k] = normal[k];
  }
  main.look = &blast_look();
  main.seed = seed;
  main.born_s = now_s;
  if (blasts) blasts->push_back(main);
  // The billows: smaller balls born round it a moment later, so the cloud is lobed.
  for (int i = 0; i < kBillows; ++i) {
    double d[3];
    unit(r, d);
    d[1] = std::abs(d[1]) * 0.6 + 0.2;  // up and out more than down
    if (normal != nullptr) away_from(normal, d);
    gr::Blast b = main;
    b.on_surface = false;  // round the dome, in the air
    const double out = rnd(r, 0.55, 0.95);
    for (int k = 0; k < 3; ++k) b.at[k] = p[k] + d[k] * out;
    b.look = &billow_look();
    b.scale = rnd(r, 0.8, 1.2);
    b.seed = seed * 7919u + static_cast<uint32_t>(i) + 1u;
    b.born_s = now_s + kBillowsBy * (i + 1) / kBillows * rnd(r, 0.4, 1.0);
    for (int k = 0; k < 3; ++k) b.v[k] = d[k] * rnd(r, 0.8, 1.8);
    if (blasts) blasts->push_back(b);
  }
  if (!births) return;
  // The chunks the engine's look throws (its sparks are ours, below).
  std::vector<gr::Spark> thrown(gr::blast_births_max(blast_look(), 1));
  const size_t n = gr::blast_births(main, thrown);
  for (size_t i = 0; i < n; ++i) births->push_back(gr::spark_particle(thrown[i]));
  // The classic's ember ball: fast embers flung out that fall, slow ones that hang and drift.
  for (int i = 0; i < 220; ++i) {
    double d[3];
    unit(r, d);
    if (normal != nullptr) away_from(normal, d);
    const double q[3] = {p[0] + d[0] * 0.2, p[1] + d[1] * 0.2, p[2] + d[2] * 0.2};
    Particle e = ember(q, d, rnd(r, 4, 10), 2500, rnd(r, 0.4, 0.6), rnd(r, 0.6, 1.2), r);
    e.size = float(rnd(r, 0.045, 0.07));
    births->push_back(e);
  }
  for (int i = 0; i < 120; ++i) {
    double d[3];
    unit(r, d);
    if (normal != nullptr) away_from(normal, d);
    const double q[3] = {p[0] + d[0] * 0.4, p[1] + d[1] * 0.4, p[2] + d[2] * 0.4};
    Particle e = ember(q, d, rnd(r, 1.5, 4.5), 2300, rnd(r, 0.6, 0.9), rnd(r, 0.9, 1.6), r);
    e.drag = 2.6f;
    e.fall = 0.2f;
    e.size = float(rnd(r, 0.06, 0.09));
    births->push_back(e);
  }
  // White-hot sparks, fast and streaked.
  for (int i = 0; i < 48; ++i) {
    double d[3];
    unit(r, d);
    if (normal != nullptr) away_from(normal, d);
    Particle s = ember(p, d, rnd(r, 12, 26), 2900, 0.18, rnd(r, 0.25, 0.55), r);
    s.size = 0.02f;
    s.drag = 0.8f;
    s.fall = 1;
    s.streak = 0.045f;
    births->push_back(s);
  }
}

bool blast_light(const gr::Blast& b, double now_s, Light* out) {
  const gr::Fireball f = gr::fireball_at(b, now_s);
  if (!f.alive) return false;
  const gr::Flash flash = gr::flash_at(b, now_s);
  // The flash's share of its first moment, and a glow that falls away after it (the embers and
  // the hot smoke), gone by kExplosionLight.
  const double age = now_s - b.born_s;
  const double tail = age < kExplosionLight ? 1 - age / kExplosionLight : 0;
  const double k = std::max(f.fire, 0.35 * tail);
  if (k <= 0.01) return false;
  for (int i = 0; i < 3; ++i) out->pos[i] = f.centre[i];
  out->radius = gr::look_of(b).light_range_m * (0.6 + 0.4 * k);
  const double level = 2.6 * k;
  for (int i = 0; i < 3; ++i) out->colour[i] = level * flash.colour[i];
  return true;
}

namespace {

// A swell to 1 at `tau` s that dies away after it: (t / tau) e^(1 - t / tau).
double swell(double t, double tau) { return t <= 0 ? 0 : t / tau * std::exp(1 - t / tau); }

}  // namespace

void launch_offset(double age, const double dir[3], const RocketLaunch& l, double out[3]) {
  out[0] = out[1] = out[2] = 0;
  if (age <= 0 || age >= kLaunchS) return;
  // Gone by kLaunchS whatever the curves say.
  const double u = std::clamp((age - 0.3) / (kLaunchS - 0.3), 0.0, 1.0);
  const double settle = 1 - u * u * (3 - 2 * u);
  // Behind its true place along the line (the slow pop, then the motor catching it up), a little
  // under it (the drop before the motor bites), and the shooter's sway.
  const double lag = 0.45 * swell(age, 0.09), drop = 0.22 * swell(age, 0.13);
  const double kick = swell(age, 0.12);
  // A waver across the line off its own seed, dying as it gathers speed.
  ge::core::Rng rng = ge::core::make_rng(l.seed * 2654435761u + 17u);
  const double phase = ge::core::next(rng) * 6.283185307179586;
  const double turn = ge::core::next(rng) * 6.283185307179586;
  double a[3], b[3];
  // Two axes square to the flight.
  const double up[3] = {0, 1, 0};
  a[0] = dir[1] * up[2] - dir[2] * up[1];
  a[1] = dir[2] * up[0] - dir[0] * up[2];
  a[2] = dir[0] * up[1] - dir[1] * up[0];
  double al = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  if (al < 1e-6) {
    a[0] = 1, a[1] = a[2] = 0;
    al = 1;
  }
  for (double& c : a) c /= al;
  b[0] = dir[1] * a[2] - dir[2] * a[1];
  b[1] = dir[2] * a[0] - dir[0] * a[2];
  b[2] = dir[0] * a[1] - dir[1] * a[0];
  const double waver = 0.035 * std::sin(age * 2 * 3.14159265358979 * 7.5 + phase) *
                       std::exp(-age / 0.18) * std::min(age / 0.03, 1.0);
  const double wa = std::cos(turn) * waver, wb = std::sin(turn) * waver;
  for (int k = 0; k < 3; ++k)
    out[k] = settle * (-dir[k] * lag + l.kick[k] * kick + a[k] * wa + b[k] * wb) -
             (k == 1 ? settle * drop : 0);
}

void launch_kick(const double dir[3], const double v[3], double yaw_rate, double pitch_rate,
                 double out[3]) {
  // His movement across the line, carried a moment; along it the rocket is his speed ahead anyway.
  const double along = v[0] * dir[0] + v[1] * dir[1] + v[2] * dir[2];
  for (int k = 0; k < 3; ++k) out[k] = (v[k] - along * dir[k]) * 0.08;
  // His aim's swing: the rocket left the tube pointing where the aim was, so it lags behind the
  // turn (right of the line for a turn to the left).
  const double h = std::atan2(dir[0], dir[2]);
  const double right[3] = {std::cos(h), 0, -std::sin(h)};
  for (int k = 0; k < 3; ++k) out[k] -= right[k] * yaw_rate * 0.12;
  out[1] -= pitch_rate * 0.12;
  const double n = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
  if (n > kKickM)
    for (int k = 0; k < 3; ++k) out[k] *= kKickM / n;
}

Light motor_light(const double at[3], const double dir[3], uint32_t seed, double now_s) {
  Light l;
  for (int k = 0; k < 3; ++k) l.pos[k] = at[k] - dir[k] * kNozzleM;
  l.radius = kMotorLightM;
  // Two incommensurate wavers, phased off the seed: a flame's flicker, never a strobe.
  const double phase = static_cast<double>(seed % 997u) * 0.37;
  const double waver = 0.5 + 0.3 * std::sin(now_s * 2 * 3.14159265358979 * 8.3 + phase) +
                       0.2 * std::sin(now_s * 2 * 3.14159265358979 * 11.9 + phase * 1.7);
  const double level = kMotorLevel * (0.8 + 0.2 * waver);
  constexpr double kFlame[3] = {1.0, 0.66, 0.34};  // a motor's flame, about 1900 K
  for (int k = 0; k < 3; ++k) l.colour[k] = level * kFlame[k];
  return l;
}

gr::MuzzleFire exhaust(const double p[3], const double dir[3], uint32_t seed, double now_s) {
  gr::MuzzleFire f;
  f.shot.look = &exhaust_look();
  f.shot.seed = seed;
  f.shot.born_s = now_s - 0.012;  // a flame always young: re-lit every frame
  for (int k = 0; k < 3; ++k) {
    f.pose.at[k] = p[k];
    f.pose.dir[k] = -dir[k];
  }
  // Any up square to the way it points.
  const double up[3] = {std::abs(dir[1]) < 0.9 ? 0.0 : 1.0, std::abs(dir[1]) < 0.9 ? 1.0 : 0.0, 0};
  const double along = up[0] * dir[0] + up[1] * dir[1];
  double u[3] = {up[0] - along * dir[0], up[1] - along * dir[1], -along * dir[2]};
  const double l = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  for (int k = 0; k < 3; ++k) f.pose.up[k] = u[k] / l;
  return f;
}

namespace {

void trail(const double from[3], const double to[3], uint32_t seed, bool rocket, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  const double step = rocket ? kRocketStep : kGrenadeStep;
  const int n = std::max(1, int(len / step));
  for (int i = 0; i < n; ++i) {
    const double t = (i + rnd(r, 0, 1)) / n;
    const double p[3] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t};
    if (rocket) {
      // Fire shed behind the motor: hot, slow, cooling to nothing within a few tenths.
      double dir[3];
      unit(r, dir);
      Particle e = ember(p, dir, rnd(r, 0.2, 0.9), 2100, 0.09, rnd(r, 0.2, 0.4), r);
      e.fall = -0.1f;
      e.size = float(rnd(r, 0.05, 0.09));
      e.streak = 0;
      out->push_back(e);
      // Puffs of every size, some thick, some thin, spreading at their own rates: a trail of
      // smoke, not a tube.
      Particle s = puff(p, rnd(r, 0.05, 0.15), rnd(r, 0.25, 0.9), 0.32, rnd(r, 0.15, 0.5),
                        rnd(r, 0.9, 2.2), kWarmGrey, r);
      for (float& v : s.v) v *= 2.5f;
      out->push_back(s);
    } else {
      out->push_back(puff(p, rnd(r, 0.04, 0.1), rnd(r, 0.2, 0.5), 0.45, rnd(r, 0.15, 0.35),
                          rnd(r, 0.7, 1.3), kPaleGrey, r));
    }
  }
}

}  // namespace

void fire_trail(const double from[3], const double to[3], uint32_t seed, Particles* out) {
  trail(from, to, seed, true, out);
}

void smoke_trail(const double from[3], const double to[3], uint32_t seed, Particles* out) {
  trail(from, to, seed, false, out);
}

void hit(const WallHit& w, uint32_t seed, double now_s, std::vector<gr::Blast>* blasts,
         Particles* births) {
  const gr::ImpactLook* look = gr::impact_look_for(w.surface);
  if (look == nullptr) return;  // the sky
  gr::Impact h;
  for (int k = 0; k < 3; ++k) {
    h.at[k] = w.at[k];
    h.normal[k] = w.normal[k];
    h.dir[k] = w.dir[k];
  }
  h.look = look;
  // About the rounds' own size: subtle, a nail's bite a little deeper than a pellet's.
  static const gr::ImpactRound kNail = {"se.round.nail", 1.2, 1.0};
  static const gr::ImpactRound kPellet = {"se.round.pellet", 0.9, 0.35};
  h.round = w.nail ? &kNail : &kPellet;
  h.seed = seed;
  h.born_s = now_s;
  gr::Blast dust;
  if (blasts && gr::dust_of(h, dust)) blasts->push_back(dust);
  if (!births) return;
  // MONOTONE: what comes off the wall is its brightness in greys, a hint of its hue at most
  // (kHueKept), so a hit reads as dust and grit, never as coloured confetti.
  const bool coloured = w.colour[0] >= 0;
  const float lum =
      coloured ? 0.2126f * w.colour[0] + 0.7152f * w.colour[1] + 0.0722f * w.colour[2] : 0;
  constexpr float kHueKept = 0.12f;
  const auto paint = [&](Particle& q) {
    q.floor_y = static_cast<float>(w.floor_y);
    if (!coloured || lum <= 1e-4f) return;
    q.albedo = std::clamp(lum, 0.06f, 0.8f);
    for (int k = 0; k < 3; ++k) q.tint[k] = 1 + kHueKept * (std::min(w.colour[k] / lum, 2.0f) - 1);
  };
  std::vector<gr::Spark> thrown(gr::impact_births_max(*look, gr::round_of(h)));
  const size_t n = gr::impact_births(h, thrown);
  for (size_t i = 0; i < n; ++i) {
    Particle q = gr::spark_particle(thrown[i]);
    if (thrown[i].kind == gr::SparkKind::spark) {
      // Sparks only from a nail off metal: anything else's hit stays in its greys.
      if (w.surface != "metal" || !w.nail) continue;
      q.floor_y = static_cast<float>(w.floor_y);
      q.bounce = 0.25f;
    } else {
      paint(q);  // chips, grit, a splash's beads: monotone; the chips hop
      if (thrown[i].kind == gr::SparkKind::chip) q.bounce = 0.3f;
    }
    births->push_back(q);
  }
  Rng r = ge::core::make_rng(seed ^ 0x5bd1e995u);
  // Flakes: bits of the face knocked loose, out of it slowly, falling to the floor and hopping.
  const int flakes = w.nail ? 5 : 3;
  for (int i = 0; i < flakes; ++i) {
    double d[3];
    unit(r, d);
    for (int k = 0; k < 3; ++k) d[k] = 0.45 * d[k] + w.normal[k];
    Particle q = at(w.at);
    set_v(q, d, rnd(r, 0.5, 1.5));
    q.life = float(rnd(r, 1.8, 2.8));
    q.size = float(rnd(r, 0.015, 0.03));
    q.drag = 0.3f;
    q.fall = 1;
    q.streak = 0.004f;
    q.albedo = 0.35f;
    q.opacity = 1;
    q.bounce = 0.3f;
    paint(q);
    births->push_back(q);
  }
  // Dust: a small cloud of the wall's own powder kicked out of the hole, spreading as it slows and
  // thinning away within a second.
  const int motes = w.nail ? 4 : 3;
  for (int i = 0; i < motes; ++i) {
    double d[3];
    unit(r, d);
    for (int k = 0; k < 3; ++k) d[k] = 0.6 * d[k] + w.normal[k];
    const double off[3] = {w.at[0] + w.normal[0] * 0.025, w.at[1] + w.normal[1] * 0.025,
                           w.at[2] + w.normal[2] * 0.025};
    Particle q = at(off);
    set_v(q, d, rnd(r, 0.6, 1.6));
    q.life = float(rnd(r, 0.6, 1.1));
    q.size = float(rnd(r, 0.03, 0.06));
    q.grow = float(rnd(r, 0.2, 0.4));
    q.drag = 5;
    q.fall = 0.05f;
    q.albedo = 0.5f;
    q.opacity = 0.3f;
    paint(q);
    q.albedo = std::max(q.albedo, 0.35f);            // powder is paler than the face it came off
    for (float& c : q.tint) c = 1 + 0.5f * (c - 1);  // and greyer still
    births->push_back(q);
  }
  // The wisp: a thin, slow smoke that drifts off the face and spreads.
  Particle s = puff(w.at, 0.05, 0.12, 0.45, 0.1, rnd(r, 1.4, 2.2), kPaleGrey, r);
  for (int k = 0; k < 3; ++k) s.v[k] += float(w.normal[k] * 0.15);
  births->push_back(s);
}

void bolt_hit(const double p[3], const double n[3], uint32_t seed, Particles* out) {
  Rng r = ge::core::make_rng(seed);
  for (int i = 0; i < 6; ++i) {
    double d[3];
    unit(r, d);
    away_from(n, d);
    // Hotter than any fire: a blue-white that holds while it flies.
    Particle s = ember(p, d, rnd(r, 3, 9), 9000, 0.5, rnd(r, 0.08, 0.2), r);
    s.size = 0.015f;
    s.streak = 0.03f;
    s.fall = 0.5f;
    out->push_back(s);
  }
}

bool spark_light(const double at[3], double age, Light* out) {
  if (age < 0 || age >= kSparkLightS) return false;
  const double left = 1 - age / kSparkLightS;
  for (int k = 0; k < 3; ++k) out->pos[k] = at[k];
  out->radius = 1.5;
  constexpr double kSpark[3] = {1.0, 0.82, 0.55};  // hot steel's spark, about 2400 K
  for (int k = 0; k < 3; ++k) out->colour[k] = 0.9 * left * left * kSpark[k];
  return true;
}

Light dynamic_light(const gr::LocalLight& l) {
  Light d;
  for (int k = 0; k < 3; ++k) d.pos[k] = l.position[k];
  d.radius = l.range_m;
  const double level = std::min(static_cast<double>(l.candela) / 2500.0, 1.2);
  for (int k = 0; k < 3; ++k) d.colour[k] = level * l.colour[k];
  return d;
}

}  // namespace shooters_edge::effects
