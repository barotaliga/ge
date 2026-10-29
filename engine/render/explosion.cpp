// The presets and the knobs' table, the fireball's curves, the flash, the births dealt off the
// seed, the scorch.
//
// ge:summary Explosion definitions: the presets, the knobs, the curves of a blast's life, births
// ge:keywords render explosion blast fireball flash sparks scorch presets knobs curves seed
// ge:skill render
// ge:test tests/render_explosion_test.cpp
#include "render/explosion.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "render/blackbody.hpp"

namespace ge::render {

namespace {

namespace lm = ge::core::libm;
constexpr double kPi = 3.14159265358979323846;

// The streams a blast's draws come off: its births and its scorch, each its own, so a change to
// what it throws never turns its mark.
constexpr uint32_t kBirthSalt = 0x9e3779b9u;
constexpr uint32_t kScorchSalt = 0x7f4a7c15u;

// The engine's presets: the common blast, then the ones a content table picks for a kind of
// weapon or a size. Each is the default look with what makes it itself changed.
constexpr BlastLook kCommon{};
constexpr BlastLook kRocket{.id = "explosion.rocket",
                            .radius_m = 1.25,
                            .grow_s = 0.05,
                            .rise = 1.0,
                            .fire_s = 0.38,
                            .core_k = 2600,
                            .cool_s = 0.4,
                            .flash = 0.6,
                            .smoke = 0.7,
                            .smoke_s = 3.0,
                            .fray = 0.35,
                            .roll = 0.6,
                            .sparks = 96,
                            .spark_speed = 34,
                            .spark_life_s = 0.7,
                            .spark_k = 2650,
                            .debris = 8,
                            .scorch_m = 1.4,
                            .light_range_m = 22};
constexpr BlastLook kGrenade{.id = "explosion.grenade",
                             .radius_m = 1.6,
                             .grow_s = 0.09,
                             .smoke_grow = 0.6,
                             .rise = 0.6,
                             .fire = 0.55,
                             .fire_s = 0.28,
                             .core_k = 2050,
                             .cool_s = 0.3,
                             .flash = 0.35,
                             .smoke = 1.4,
                             .smoke_in_s = 0.18,
                             .smoke_s = 4.4,
                             .smoke_albedo = 0.38,
                             .smoke_tint = {0.86f, 0.74f, 0.58f},
                             .billows = 1.0,
                             .fray = 0.45,
                             .billow_size = 1.35,
                             .roll = 0.27,
                             .sparks = 18,
                             .spark_speed = 17,
                             .spark_life_s = 0.6,
                             .spark_k = 2100,
                             .debris = 40,
                             .debris_hot = 0.12,
                             .scorch_m = 1.9,
                             .light_range_m = 12};
constexpr BlastLook kSmall{.id = "explosion.small",
                           .radius_m = 0.9,
                           .core_k = 2200,
                           .smoke_s = 2.8,
                           .sparks = 28,
                           .spark_speed = 16,
                           .debris = 6,
                           .scorch_m = 1.1,
                           .light_range_m = 12};
constexpr BlastLook kLarge{.id = "explosion.large",
                           .radius_m = 2.6,
                           .grow_s = 0.1,
                           .rise = 0.75,
                           .fire_s = 0.7,
                           .core_k = 2450,
                           .cool_s = 0.7,
                           .smoke = 1.3,
                           .smoke_s = 5.0,
                           .smoke_albedo = 0.08,
                           .billow_size = 0.85,
                           .sparks = 80,
                           .spark_speed = 30,
                           .spark_life_s = 1.1,
                           .debris = 24,
                           .scorch_m = 3.0,
                           .light_range_m = 30};
constexpr std::array<BlastLook, 5> kLooks{kCommon, kRocket, kGrenade, kSmall, kLarge};

constexpr std::array<BlastKnob, 26> kKnobs{{
    {"radius", "m", "the fireball's radius once swollen", &BlastLook::radius_m, nullptr, 0.1, 20},
    {"grow", "s", "how fast it swells (the swell's time constant)", &BlastLook::grow_s, nullptr,
     0.01, 1},
    {"smoke_grow", "", "how much more it grows as smoke, a share of its radius",
     &BlastLook::smoke_grow, nullptr, 0, 2},
    {"rise", "radii/s", "how fast it rises once going", &BlastLook::rise, nullptr, 0, 4},
    {"fire", "", "how much of the ball burns at the start", &BlastLook::fire, nullptr, 0, 1},
    {"fire_time", "s", "how long the fire lasts (its e-folding time)", &BlastLook::fire_s, nullptr,
     0.05, 5},
    {"heat", "K", "the core's heat as it goes off (white over 2500, orange at 1800)",
     &BlastLook::core_k, nullptr, 900, 3200},
    {"cool_to", "K", "the heat it cools toward (a dull red at 1300)", &BlastLook::cool_k, nullptr,
     700, 2500},
    {"cool_time", "s", "how fast its heat falls", &BlastLook::cool_s, nullptr, 0.05, 5},
    {"flash", "", "how bright its light is, a share of a blackbody's", &BlastLook::flash, nullptr,
     0, 1},
    {"smoke", "", "how thick the smoke is", &BlastLook::smoke, nullptr, 0, 2},
    {"smoke_build", "s", "how long the smoke takes to build", &BlastLook::smoke_in_s, nullptr, 0.02,
     3},
    {"smoke_time", "s", "until the last smoke is gone (the blast's life)", &BlastLook::smoke_s,
     nullptr, 0.3, 30},
    {"smoke_albedo", "", "how pale the smoke is: soot 0.1, dust 0.4, steam 0.9",
     &BlastLook::smoke_albedo, nullptr, 0, 0.98},
    {"billows", "", "how deep its big billows cut", &BlastLook::billows, nullptr, 0, 1.5},
    {"fray", "", "the fine fraying at its edge", &BlastLook::fray, nullptr, 0, 1},
    {"billow_size", "", "the billows' size against the ball's", &BlastLook::billow_size, nullptr,
     0.4, 3},
    {"roll", "radii/s", "how fast it turns over", &BlastLook::roll, nullptr, 0, 3},
    {"sparks", "", "sparks thrown at scale 1", nullptr, &BlastLook::sparks, 0, 400},
    {"spark_speed", "m/s", "the fastest spark", &BlastLook::spark_speed, nullptr, 1, 120},
    {"spark_life", "s", "a spark's mean life", &BlastLook::spark_life_s, nullptr, 0.05, 5},
    {"spark_heat", "K", "a spark's heat at birth", &BlastLook::spark_k, nullptr, 900, 3200},
    {"debris", "", "chunks thrown at scale 1", nullptr, &BlastLook::debris, 0, 200},
    {"debris_hot", "", "the share of chunks still glowing", &BlastLook::debris_hot, nullptr, 0, 1},
    {"scorch", "m", "the burnt mark's size across (0: none)", &BlastLook::scorch_m, nullptr, 0, 20},
    {"light_range", "m", "where the flash's light ends", &BlastLook::light_range_m, nullptr, 0,
     200},
}};

double smoothstep(double a, double b, double x) {
  const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

double age_of(const Blast& b, double now_s) { return now_s - b.born_s; }

// A direction off the stream: anywhere on the sphere in the open air, off a surface into the
// half space it faces, leaning out along its normal.
void direction(core::Rng& rng, const Blast& b, double lean, double d[3]) {
  const double z = core::range(rng, -1, 1), a = core::range(rng, 0, 2 * kPi);
  const double rxy = std::sqrt(std::max(0.0, 1 - z * z));
  d[0] = rxy * lm::cos(a);
  d[1] = z;
  d[2] = rxy * lm::sin(a);
  if (!b.on_surface) return;
  double dn = d[0] * b.normal[0] + d[1] * b.normal[1] + d[2] * b.normal[2];
  if (dn < 0) {
    for (int k = 0; k < 3; ++k) d[k] -= 2 * dn * b.normal[k];
  }
  for (int k = 0; k < 3; ++k) d[k] += lean * b.normal[k];
  const double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (int k = 0; k < 3; ++k) d[k] /= l;
}

uint32_t scaled(uint32_t n, double scale) {
  return static_cast<uint32_t>(std::lround(n * std::clamp(scale * scale, 0.05, 16.0)));
}

}  // namespace

std::span<const BlastLook> blast_looks() { return kLooks; }

const BlastLook& default_look() { return kLooks[0]; }

const BlastLook* blast_look_by_id(std::string_view id) {
  for (const BlastLook& l : kLooks) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

std::span<const BlastKnob> blast_knobs() { return kKnobs; }

bool set_knob(BlastLook& look, std::string_view name, double value) {
  for (const BlastKnob& k : kKnobs) {
    if (k.name != name) continue;
    const double v = std::clamp(value, k.lo, k.hi);
    if (k.number) look.*k.number = v;
    if (k.count) look.*k.count = static_cast<uint32_t>(std::lround(v));
    return true;
  }
  return false;
}

double blast_life(const Blast& b) { return look_of(b).smoke_s * std::sqrt(b.scale); }

Fireball fireball_at(const Blast& b, double now_s) {
  const BlastLook& k = look_of(b);
  const double t = age_of(b, now_s), life = blast_life(b);
  Fireball f{};
  f.age_s = t;
  f.alive = t >= 0 && t < life;
  const double tc = std::max(t, 0.0);
  const double big = k.radius_m * b.scale;
  // Swollen in a fifth of a second, then growing on as smoke.
  f.radius_m = big * std::sqrt(-lm::expm1(-tc / (k.grow_s * std::sqrt(b.scale)))) -
               big * k.smoke_grow * lm::expm1(-tc / 1.6);
  f.radius_m = std::max(f.radius_m, 0.05 * big);
  // Hot air rises: slowly at first, then steadily.
  const double lift = big * k.rise * tc * tc / (tc + 0.7);
  const double out = b.on_surface ? big / 3 : 0;
  for (int i = 0; i < 3; ++i) f.centre[i] = b.at[i] + out * b.normal[i];
  f.centre[1] += lift;
  // Thrown (a muzzle's puff): carried on, slowing in the air.
  const double carried = -kBlastDragS * lm::expm1(-tc / kBlastDragS);
  for (int i = 0; i < 3; ++i) f.centre[i] += b.v[i] * carried;
  f.core_k = k.cool_k + (k.core_k - k.cool_k) * lm::exp(-tc / k.cool_s);
  f.fire = k.fire * lm::exp(-tc / k.fire_s);
  f.smoke = k.smoke * -lm::expm1(-tc / k.smoke_in_s) * (1 - smoothstep(0.35 * life, life, tc));
  return f;
}

Flash flash_at(const Blast& b, double now_s) {
  const Fireball f = fireball_at(b, now_s);
  Flash out{};
  for (int i = 0; i < 3; ++i) out.at[i] = f.centre[i];
  const Blackbody c = blackbody(f.core_k);
  for (int i = 0; i < 3; ++i) out.colour[i] = c.rgb[i];
  const BlastLook& k = look_of(b);
  out.candela =
      f.alive ? kPi * f.radius_m * f.radius_m * blackbody_cd(f.core_k) * f.fire * k.flash : 0;
  out.range_m = k.light_range_m * b.scale;
  out.radius_m = std::max(0.5 * f.radius_m, 0.05);
  return out;
}

size_t blast_births_max(const BlastLook& k, double scale) {
  return size_t{scaled(k.sparks, scale)} + scaled(k.debris, scale);
}

size_t blast_births(const Blast& b, std::span<Spark> out) {
  const BlastLook& k = look_of(b);
  core::Rng rng = core::make_rng(b.seed ^ kBirthSalt);
  const double big = k.radius_m * b.scale, root = std::sqrt(b.scale);
  size_t n = 0;
  const auto put = [&](const Spark& s) {
    if (n < out.size()) out[n++] = s;
  };
  double d[3];
  for (uint32_t i = 0, m = scaled(k.sparks, b.scale); i < m; ++i) {
    direction(rng, b, 0.35, d);
    const double speed = k.spark_speed * root * (0.3 + 0.7 * std::sqrt(core::next(rng)));
    const double from = 0.15 * big * core::next(rng);
    Spark s{SparkKind::spark,
            {},
            {},
            k.spark_life_s * core::range(rng, 0.5, 1.45),
            0.035 * root,
            k.spark_k + core::range(rng, -350, 350),
            core::range(rng, 0.22, 0.5)};
    for (int j = 0; j < 3; ++j) {
      s.p[j] = b.at[j] + d[j] * from;
      s.v[j] = d[j] * speed;
    }
    put(s);
  }
  for (uint32_t i = 0, m = scaled(k.debris, b.scale); i < m; ++i) {
    direction(rng, b, 0.6, d);
    const double speed = k.spark_speed * root * core::range(rng, 0.25, 0.55);
    const bool hot = core::chance(rng, k.debris_hot);
    Spark s{
        SparkKind::debris,  {}, {}, core::range(rng, 1.2, 2.2), core::range(rng, 0.05, 0.12) * root,
        hot ? 1400.0 : 0.0, 0.7};
    for (int j = 0; j < 3; ++j) {
      s.p[j] = b.at[j] + d[j] * 0.1 * big;
      s.v[j] = d[j] * speed;
    }
    put(s);
  }
  return n;
}

bool scorch_of(const Blast& b, Scorch& s) {
  if (!b.on_surface || look_of(b).scorch_m <= 0) return false;
  core::Rng rng = core::make_rng(b.seed ^ kScorchSalt);
  s = Scorch{};
  for (int i = 0; i < 3; ++i) {
    s.at[i] = b.at[i];
    s.normal[i] = b.normal[i];
  }
  s.size_m = look_of(b).scorch_m * b.scale;
  s.spin_rad = core::range(rng, 0, 2 * kPi);
  s.variant = static_cast<uint32_t>(core::int_in(rng, 0, kScorchVariants - 1));
  s.born_s = b.born_s;
  return true;
}

}  // namespace ge::render
