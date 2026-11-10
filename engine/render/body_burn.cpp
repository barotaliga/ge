// A body burned by a hot liquid or eaten by an acid one (body_burn.hpp): the lags, the births.
//
// ge:summary A body burn's lags and what a burning or acid-wet body throws, a frame at a time
// ge:keywords render scorch step_body_burn body_burn_births smoke flames embers fumes spots
// ge:skill render
// ge:test tests/render_body_burn_test.cpp
#include "render/body_burn.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::render {

namespace {

namespace lm = core::libm;

constexpr int kSpots = 6;          // where a body's flames burn: fixed by its seed
constexpr double kLastFall = 0.1;  // 1/s: heat and acid fall at least this fast once out
constexpr double kTwoPi = 6.283185307179586;

double rnd(core::Rng& r, double lo, double hi) { return core::range(r, lo, hi); }

// A first-order lag of `x` toward `to` over `dt` with time constant `tau`.
float lag(float x, double to, double tau, double dt) {
  const double k = 1 - lm::exp(-dt / std::max(tau, 1e-3));
  return static_cast<float>(std::clamp(x + (to - x) * k, 0.0, 1.0));
}

// Spot `i` of the body seeded `seed`: its height share, its angle round the body (rad, from the
// way it faces) and how fast it flares and dies (Hz).
void spot(uint32_t seed, int i, double& up, double& angle, double& hz) {
  core::Rng r = core::make_rng(seed * 2654435761u + static_cast<uint32_t>(i) * 40503u + 7u);
  up = rnd(r, 0.15, 0.85);
  angle = kTwoPi * (static_cast<double>(i) + rnd(r, 0.1, 0.9)) / kSpots;
  hz = rnd(r, 0.6, 1.4);
}

// A point on the body's skin at `up` of its height, `angle` round it.
void on_skin(const BurnShape& b, double up, double angle, double out[3]) {
  const double a = b.heading + angle;
  out[0] = b.feet[0] + b.radius_m * lm::sin(a);
  out[1] = b.feet[1] + b.height_m * up;
  out[2] = b.feet[2] + b.radius_m * lm::cos(a);
}

Particle base(const double p[3], const BurnShape& b, double carry) {
  Particle q{};
  for (int k = 0; k < 3; ++k) {
    q.p[k] = static_cast<float>(p[k]);
    q.v[k] = static_cast<float>(b.v[k] * carry);
  }
  q.age = 0;
  return q;
}

// Births owed by `debt` after `rate` a second over `dt`: the whole ones, the rest carried.
int owed(double& debt, double rate, double dt) {
  debt += std::max(rate, 0.0) * dt;
  const int n = static_cast<int>(debt);
  debt -= n;
  return std::min(n, 64);
}

}  // namespace

const BodyBurnLook& body_burn_look() {
  static const BodyBurnLook look{};
  return look;
}

BodyBurn make_body_burn(uint32_t seed) {
  BodyBurn s;
  s.seed = seed;
  s.rng = core::make_rng(seed ^ 0x5bd1e995u);
  return s;
}

void step_body_burn(BodyBurn& s, const BodyBurnLook& look, double hot, double acid, double dt) {
  if (dt <= 0) return;
  hot = std::clamp(hot, 0.0, 1.0);
  acid = std::clamp(acid, 0.0, 1.0);
  // Heat toward how deep it is, quickly in, slower out, and once out a last fall to nothing.
  s.heat = lag(s.heat, hot, hot > s.heat ? look.heat_in_s : look.heat_out_s, dt);
  if (hot <= 0) s.heat = static_cast<float>(std::max(s.heat - dt * kLastFall, 0.0));
  // The char builds while hot (toward all of it, faster the hotter), and wears off at an even
  // pace once the heat is under it: a whole char in char_out_s.
  if (s.heat > s.charred)
    s.charred = lag(s.charred, 1.0, look.char_in_s / std::max(double(s.heat), 0.05), dt);
  else
    s.charred = static_cast<float>(std::max(s.charred - dt / look.char_out_s, double(s.heat)));
  s.acid = lag(s.acid, acid, acid > s.acid ? look.acid_in_s : look.acid_out_s, dt);
  if (acid <= 0) s.acid = static_cast<float>(std::max(s.acid - dt * kLastFall, 0.0));
  if (s.acid > s.etched)
    s.etched = lag(s.etched, s.acid, look.etch_in_s, dt);
  else
    s.etched = static_cast<float>(std::max(s.etched - dt / look.etch_out_s, double(s.acid)));
  // Under a hundredth nothing shows: let it go to rest.
  for (float* v : {&s.heat, &s.charred, &s.acid, &s.etched})
    if (*v < 0.01f) *v = 0;
}

bool body_burn_cold(const BodyBurn& s) {
  return s.heat <= 0 && s.charred <= 0 && s.acid <= 0 && s.etched <= 0;
}

size_t body_burn_births(BodyBurn& s, const BodyBurnLook& look, const BurnShape& b, double dt,
                        std::vector<Particle>& out) {
  if (dt <= 0 || body_burn_cold(s)) return 0;
  const size_t before = out.size();
  core::Rng& r = s.rng;
  const double floor_y = b.level_y + 0.03;  // nothing is born inside the liquid
  // SMOKE: thick and black off a burning body, thinner and greyer as it cools.
  const double heat = s.heat;
  const int smoke = owed(s.debt[0], look.smoke_per_s * std::max(heat, s.charred / 3.0), dt);
  for (int i = 0; i < smoke; ++i) {
    double p[3];
    on_skin(b, rnd(r, 0.25, 1.0), rnd(r, 0, kTwoPi), p);
    p[1] = std::max(p[1], floor_y);
    Particle q = base(p, b, 0.3);
    q.v[0] += static_cast<float>(rnd(r, -0.15, 0.15));
    q.v[1] += static_cast<float>(rnd(r, 0.45, 0.9));
    q.v[2] += static_cast<float>(rnd(r, -0.15, 0.15));
    q.life = static_cast<float>(rnd(r, 1.4, 2.2));
    q.size = static_cast<float>(rnd(r, 0.16, 0.26));
    q.grow = static_cast<float>(rnd(r, 0.5, 0.8));
    q.drag = 1.5f;
    q.fall = -0.05f;  // warm: it rises
    q.albedo = static_cast<float>(0.025 + 0.25 * (1 - heat));
    q.opacity = static_cast<float>(0.75 * (0.4 + 0.6 * std::max(heat, double(s.charred))));
    q.swirl = 0.25f;
    out.push_back(q);
  }
  // FLAMES: licks off a handful of spots fixed on the body, the weakest out first as it cools.
  const int licks = owed(s.debt[1], look.flame_per_s * heat * heat, dt);
  for (int i = 0; i < licks; ++i) {
    const int k = core::int_in(r, 0, kSpots - 1);
    double up, angle, hz;
    spot(s.seed, k, up, angle, hz);
    if (heat < 0.15 + 0.1 * k / double(kSpots)) continue;  // the weakest spots go out first
    double p[3];
    on_skin(b, up + rnd(r, -0.06, 0.06), angle + rnd(r, -0.35, 0.35), p);
    if (p[1] < floor_y) continue;
    Particle q = base(p, b, 0.6);
    q.v[0] += static_cast<float>(rnd(r, -0.2, 0.2));
    q.v[1] += static_cast<float>(rnd(r, 0.9, 1.6));
    q.v[2] += static_cast<float>(rnd(r, -0.2, 0.2));
    q.life = static_cast<float>(rnd(r, 0.18, 0.34) * (0.6 + 0.4 * hz));
    q.size = static_cast<float>(rnd(r, 0.06, 0.1));
    q.grow = 0.15f;
    q.streak = 0.08f;
    q.drag = 2;
    q.fall = -0.2f;
    q.albedo = 0.05f;
    q.opacity = 0.7f;
    q.kelvin = static_cast<float>(look.flame_k * rnd(r, 0.92, 1.08));
    q.cool_s = 0.16f;
    q.swirl = 0.4f;
    out.push_back(q);
  }
  // EMBERS: a few bright flecks thrown up off the char.
  const int embers = owed(s.debt[2], look.ember_per_s * heat, dt);
  for (int i = 0; i < embers; ++i) {
    double p[3];
    on_skin(b, rnd(r, 0.2, 0.9), rnd(r, 0, kTwoPi), p);
    if (p[1] < floor_y) continue;
    Particle q = base(p, b, 0.5);
    q.v[0] += static_cast<float>(rnd(r, -0.5, 0.5));
    q.v[1] += static_cast<float>(rnd(r, 1.0, 2.0));
    q.v[2] += static_cast<float>(rnd(r, -0.5, 0.5));
    q.life = static_cast<float>(rnd(r, 0.6, 1.2));
    q.size = 0.015f;
    q.streak = 0.03f;
    q.drag = 1;
    q.fall = 0.15f;
    q.albedo = 0.05f;
    q.opacity = 1;
    q.kelvin = static_cast<float>(look.flame_k * rnd(r, 0.85, 0.95));
    q.cool_s = 0.5f;
    q.swirl = 0.3f;
    out.push_back(q);
  }
  // FUMES: pale yellow-green vapour off an acid-wet body.
  const int fumes = owed(s.debt[3], look.fume_per_s * s.acid, dt);
  for (int i = 0; i < fumes; ++i) {
    double p[3];
    on_skin(b, rnd(r, 0.0, 0.8), rnd(r, 0, kTwoPi), p);
    p[1] = std::max(p[1], floor_y);
    Particle q = base(p, b, 0.3);
    q.v[0] += static_cast<float>(rnd(r, -0.1, 0.1));
    q.v[1] += static_cast<float>(rnd(r, 0.25, 0.55));
    q.v[2] += static_cast<float>(rnd(r, -0.1, 0.1));
    q.life = static_cast<float>(rnd(r, 1.0, 1.6));
    q.size = static_cast<float>(rnd(r, 0.08, 0.14));
    q.grow = static_cast<float>(rnd(r, 0.25, 0.4));
    q.drag = 1.5f;
    q.fall = -0.04f;
    q.albedo = 0.6f;
    q.tint[0] = 0.85f, q.tint[1] = 0.95f, q.tint[2] = 0.55f;
    q.opacity = static_cast<float>(0.2 * s.acid);
    q.swirl = 0.3f;
    out.push_back(q);
  }
  return out.size() - before;
}

BurnSkin body_burn_skin(const BodyBurn& s) {
  return {s.charred, s.heat, s.etched, static_cast<float>(s.seed % 1000u) / 1000.0f};
}

BurnStir body_burn_stir(const BodyBurn& s) { return {s.heat, std::max(s.heat * 0.6f, s.acid)}; }

}  // namespace ge::render
