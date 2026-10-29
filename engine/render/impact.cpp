// The impact presets, one a surface class, the knobs' table, the rounds, the dust and the throw.
//
// ge:summary Impact definitions: each surface's look, the knobs, the rounds, the dust and births
// ge:keywords render impact hit presets knobs stone metal wood dirt sand tile liquid nail pellet
// ge:skill render
// ge:test tests/render_impact_test.cpp
#include "render/impact.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"

namespace ge::render {

namespace {

namespace lm = ge::core::libm;
constexpr double kPi = 3.14159265358979323846;

constexpr uint32_t kThrowSalt = 0x7f4a7c15u;

// A puff of a surface's dust: its size, how thick, how long it hangs, its albedo and tint.
constexpr BlastLook dust(std::string_view id, double radius_m, double smoke, double smoke_s,
                         double albedo, float r, float g, float b, double rise) {
  return BlastLook{.id = id,
                   .radius_m = radius_m,
                   .grow_s = 0.03,
                   .smoke_grow = 1.8,
                   .rise = rise,
                   .fire = 0,
                   .flash = 0,
                   .smoke = smoke,
                   .smoke_in_s = 0.02,
                   .smoke_s = smoke_s,
                   .smoke_albedo = albedo,
                   .smoke_tint = {r, g, b},
                   .billows = 0.7,
                   .fray = 0.5,
                   .billow_size = 1.2,
                   .roll = 0.5,
                   .sparks = 0,
                   .debris = 0,
                   .scorch_m = 0,
                   .light_range_m = 0};
}

// Each surface's answer. Stone: a grey powder blown out, flakes, a few sparks off a steel round.
// Metal: no dust to speak of, a spray of sparks on along the round's glance. Wood: tan dust and
// splinters, no sparks. Dirt and grass: a dark plume kicked straight up and clods falling back.
// Sand: a pale fan of fines. Tile: white shards and a little powder. Liquid: a column of beads
// and a white mist.
constexpr ImpactLook kStone{
    .id = "impact.stone",
    .dust = dust("impact.stone.dust", 0.09, 0.9, 1.2, 0.5, 0.9f, 0.88f, 0.84f, 0.15),
    .dust_speed = 2.5,
    .ricochet = 0.3,
    .sparks = 3,
    .spark_speed = 9,
    .spark_life_s = 0.18,
    .spark_k = 2000,
    .spark_spread = 0.6,
    .chips = 6,
    .chip_speed = 5,
    .chip_size_m = 0.018,
    .chip_life_s = 0.7,
    .chip_albedo = 0.45,
    .chip_spread = 0.9,
    .grit = 8,
    .grit_speed = 2.5,
    .grit_albedo = 0.5};
constexpr ImpactLook kMetal{
    .id = "impact.metal",
    .dust = dust("impact.metal.dust", 0.05, 0.25, 0.5, 0.3, 0.85f, 0.88f, 0.95f, 0.3),
    .dust_speed = 1.5,
    .ricochet = 0.75,
    .sparks = 18,
    .spark_speed = 14,
    .spark_life_s = 0.28,
    .spark_k = 2300,
    .spark_spread = 0.5,
    .chips = 2,
    .chip_speed = 3,
    .chip_size_m = 0.009,
    .chip_life_s = 0.5,
    .chip_albedo = 0.2,
    .chip_spread = 0.8,
    .grit = 0,
    .grit_speed = 1,
    .grit_albedo = 0.3};
constexpr ImpactLook kWood{
    .id = "impact.wood",
    .dust = dust("impact.wood.dust", 0.08, 0.7, 1.0, 0.55, 0.95f, 0.8f, 0.6f, 0.1),
    .dust_speed = 2,
    .ricochet = 0.15,
    .sparks = 0,
    .spark_speed = 6,
    .spark_life_s = 0.15,
    .spark_k = 1800,
    .spark_spread = 0.6,
    .chips = 8,
    .chip_speed = 4,
    .chip_size_m = 0.024,
    .chip_life_s = 0.8,
    .chip_albedo = 0.55,
    .chip_spread = 0.8,
    .grit = 6,
    .grit_speed = 2,
    .grit_albedo = 0.6};
constexpr ImpactLook kDirt{
    .id = "impact.dirt",
    .dust = dust("impact.dirt.dust", 0.12, 1.3, 1.6, 0.35, 0.78f, 0.62f, 0.46f, 0.05),
    .dust_speed = 4,
    .ricochet = 0.05,
    .sparks = 0,
    .spark_speed = 6,
    .spark_life_s = 0.15,
    .spark_k = 1800,
    .spark_spread = 0.6,
    .chips = 10,
    .chip_speed = 5,
    .chip_size_m = 0.021,
    .chip_life_s = 0.9,
    .chip_albedo = 0.18,
    .chip_spread = 0.45,
    .grit = 12,
    .grit_speed = 3,
    .grit_albedo = 0.3};
constexpr ImpactLook kGrass{
    .id = "impact.grass",
    .dust = dust("impact.grass.dust", 0.1, 0.9, 1.3, 0.3, 0.72f, 0.7f, 0.5f, 0.05),
    .dust_speed = 3.5,
    .ricochet = 0.05,
    .sparks = 0,
    .spark_speed = 6,
    .spark_life_s = 0.15,
    .spark_k = 1800,
    .spark_spread = 0.6,
    .chips = 8,
    .chip_speed = 4.5,
    .chip_size_m = 0.0195,
    .chip_life_s = 0.9,
    .chip_albedo = 0.15,
    .chip_spread = 0.5,
    .grit = 8,
    .grit_speed = 2.5,
    .grit_albedo = 0.25};
constexpr ImpactLook kSand{
    .id = "impact.sand",
    .dust = dust("impact.sand.dust", 0.1, 1.0, 1.1, 0.6, 0.97f, 0.88f, 0.7f, 0.05),
    .dust_speed = 3,
    .ricochet = 0.25,
    .sparks = 0,
    .spark_speed = 6,
    .spark_life_s = 0.15,
    .spark_k = 1800,
    .spark_spread = 0.6,
    .chips = 0,
    .chip_speed = 4,
    .chip_size_m = 0.012,
    .chip_life_s = 0.6,
    .chip_albedo = 0.6,
    .chip_spread = 0.6,
    .grit = 24,
    .grit_speed = 3.5,
    .grit_albedo = 0.6};
constexpr ImpactLook kTile{
    .id = "impact.tile",
    .dust = dust("impact.tile.dust", 0.07, 0.6, 0.9, 0.65, 0.95f, 0.95f, 0.93f, 0.15),
    .dust_speed = 2.5,
    .ricochet = 0.35,
    .sparks = 1,
    .spark_speed = 8,
    .spark_life_s = 0.15,
    .spark_k = 1900,
    .spark_spread = 0.6,
    .chips = 8,
    .chip_speed = 6,
    .chip_size_m = 0.021,
    .chip_life_s = 0.7,
    .chip_albedo = 0.8,
    .chip_spread = 0.8,
    .grit = 4,
    .grit_speed = 2,
    .grit_albedo = 0.7};
constexpr ImpactLook kLiquid{
    .id = "impact.liquid",
    .dust = dust("impact.liquid.mist", 0.08, 0.5, 0.6, 0.9, 1.0f, 1.0f, 1.0f, 0.1),
    .dust_speed = 2.5,
    .ricochet = 0,
    .sparks = 0,
    .spark_speed = 6,
    .spark_life_s = 0.15,
    .spark_k = 1800,
    .spark_spread = 0.6,
    .chips = 0,
    .chip_speed = 4,
    .chip_size_m = 0.012,
    .chip_life_s = 0.6,
    .chip_albedo = 0.6,
    .chip_spread = 0.6,
    .grit = 0,
    .grit_speed = 2,
    .grit_albedo = 0.6,
    .drops = 18,
    .drop_speed = 5,
    .drop_spread = 0.3};
constexpr ImpactLook kUnknown = [] {
  ImpactLook l = kStone;
  l.id = "impact.unknown";
  return l;
}();
constexpr std::array<ImpactLook, 9> kLooks{kStone, kMetal, kWood,   kDirt,   kGrass,
                                           kSand,  kTile,  kLiquid, kUnknown};

constexpr std::array<ImpactRound, 2> kRounds{{
    {"round.nail", 1.0, 1.0},     // a steel spike: the whole burst, every spark
    {"round.pellet", 0.6, 0.35},  // a lead ball: a smaller burst, few sparks (soft metal)
}};

constexpr std::array<ImpactKnob, 19> kKnobs{{
    {"dust_speed", "m/s", "how fast the puff leaves the surface", &ImpactLook::dust_speed, nullptr,
     0, 20},
    {"ricochet", "", "the burst's axis: 0 the normal, 1 the round's glance", &ImpactLook::ricochet,
     nullptr, 0, 1},
    {"sparks", "", "sparks a hit strikes", nullptr, &ImpactLook::sparks, 0, 100},
    {"spark_speed", "m/s", "the fastest spark", &ImpactLook::spark_speed, nullptr, 0.5, 60},
    {"spark_life", "s", "a spark's mean life", &ImpactLook::spark_life_s, nullptr, 0.02, 2},
    {"spark_heat", "K", "a spark's heat at birth", &ImpactLook::spark_k, nullptr, 900, 3200},
    {"spark_spread", "rad", "the sparks' cone, its half angle", &ImpactLook::spark_spread, nullptr,
     0, 1.5},
    {"chips", "", "fragments of the surface thrown", nullptr, &ImpactLook::chips, 0, 100},
    {"chip_speed", "m/s", "the fastest chip", &ImpactLook::chip_speed, nullptr, 0.5, 40},
    {"chip_size", "m", "a chip's size", &ImpactLook::chip_size_m, nullptr, 0.002, 0.1},
    {"chip_life", "s", "a chip's mean life", &ImpactLook::chip_life_s, nullptr, 0.05, 4},
    {"chip_albedo", "", "a chip's whiteness", &ImpactLook::chip_albedo, nullptr, 0, 1},
    {"chip_spread", "rad", "the chips' cone, its half angle", &ImpactLook::chip_spread, nullptr, 0,
     1.5},
    {"grit", "", "fines that hang and drift", nullptr, &ImpactLook::grit, 0, 200},
    {"grit_speed", "m/s", "the fastest fine", &ImpactLook::grit_speed, nullptr, 0.2, 20},
    {"grit_albedo", "", "the fines' whiteness", &ImpactLook::grit_albedo, nullptr, 0, 1},
    {"drops", "", "beads of a splash", nullptr, &ImpactLook::drops, 0, 200},
    {"drop_speed", "m/s", "the fastest bead", &ImpactLook::drop_speed, nullptr, 0.5, 30},
    {"drop_spread", "rad", "the beads' cone, its half angle", &ImpactLook::drop_spread, nullptr, 0,
     1.5},
}};

double dot(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void unit(double v[3]) {
  const double l = std::sqrt(dot(v, v));
  if (l <= 0) {
    v[0] = 0;
    v[1] = 1;
    v[2] = 0;
    return;
  }
  for (int i = 0; i < 3; ++i) v[i] /= l;
}

uint32_t scaled(uint32_t n, double scale, double share = 1) {
  return static_cast<uint32_t>(std::lround(n * share * std::clamp(scale * scale, 0.05, 16.0)));
}

// A way in the cone of half angle `spread` round `axis`, more of them near it, kept out of the
// surface (mirrored back through it and lifted off it).
void in_cone(core::Rng& rng, const double axis[3], const double normal[3], double spread,
             double d[3]) {
  // Two ways square to the axis.
  double a[3] = {1, 0, 0};
  if (std::abs(axis[0]) > 0.8) {
    a[0] = 0;
    a[1] = 1;
  }
  double u[3] = {axis[1] * a[2] - axis[2] * a[1], axis[2] * a[0] - axis[0] * a[2],
                 axis[0] * a[1] - axis[1] * a[0]};
  unit(u);
  const double w[3] = {axis[1] * u[2] - axis[2] * u[1], axis[2] * u[0] - axis[0] * u[2],
                       axis[0] * u[1] - axis[1] * u[0]};
  const double off = spread * std::sqrt(core::next(rng)), round = core::range(rng, 0, 2 * kPi);
  const double ca = lm::cos(off), sa = lm::sin(off), cr = lm::cos(round), sr = lm::sin(round);
  for (int i = 0; i < 3; ++i) d[i] = axis[i] * ca + (u[i] * cr + w[i] * sr) * sa;
  const double dn = dot(d, normal);
  if (dn < 0.08) {
    for (int i = 0; i < 3; ++i) d[i] += (0.08 - (dn < 0 ? 2 * dn : dn)) * normal[i];
    unit(d);
  }
}

}  // namespace

std::span<const ImpactLook> impact_looks() { return kLooks; }

const ImpactLook& default_impact() { return kLooks[0]; }

const ImpactLook* impact_look_by_id(std::string_view id) {
  for (const ImpactLook& l : kLooks) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

const ImpactLook* impact_look_for(std::string_view surface) {
  if (surface == "sky") return nullptr;
  constexpr std::string_view kPrefix = "impact.";
  for (const ImpactLook& l : kLooks) {
    if (l.id.substr(kPrefix.size()) == surface) return &l;
  }
  return &default_impact();
}

std::span<const ImpactKnob> impact_knobs() { return kKnobs; }

bool set_impact_knob(ImpactLook& look, std::string_view name, double value) {
  constexpr std::string_view kDust = "dust.";
  if (name.substr(0, kDust.size()) == kDust) {
    return set_knob(look.dust, name.substr(kDust.size()), value);
  }
  for (const ImpactKnob& k : kKnobs) {
    if (k.name != name) continue;
    const double v = std::clamp(value, k.lo, k.hi);
    if (k.number) look.*k.number = v;
    if (k.count) look.*k.count = static_cast<uint32_t>(std::lround(v));
    return true;
  }
  return false;
}

std::span<const ImpactRound> impact_rounds() { return kRounds; }

const ImpactRound& default_round() { return kRounds[0]; }

const ImpactRound* impact_round_by_id(std::string_view id) {
  for (const ImpactRound& r : kRounds) {
    if (r.id == id) return &r;
  }
  return nullptr;
}

void impact_axis(const Impact& h, double ricochet, double out[3]) {
  // The round's glance: its way mirrored in the surface.
  const double dn = dot(h.dir, h.normal);
  double glance[3];
  for (int i = 0; i < 3; ++i) glance[i] = h.dir[i] - 2 * dn * h.normal[i];
  const double r = std::clamp(ricochet, 0.0, 1.0);
  for (int i = 0; i < 3; ++i) out[i] = h.normal[i] * (1 - r) + glance[i] * r;
  // Never along the surface: lifted a little off it.
  const double on = dot(out, h.normal);
  if (on < 0.15) {
    for (int i = 0; i < 3; ++i) out[i] += (0.15 - on) * h.normal[i];
  }
  unit(out);
}

bool dust_of(const Impact& h, Blast& out) {
  const ImpactLook& k = look_of(h);
  if (k.dust.smoke <= 0) return false;
  const double scale = round_of(h).scale;
  double axis[3];
  impact_axis(h, 0.5 * k.ricochet, axis);
  out = Blast{};
  for (int i = 0; i < 3; ++i) {
    out.at[i] = h.at[i] + h.normal[i] * 0.02;
    out.normal[i] = h.normal[i];
    out.v[i] = axis[i] * k.dust_speed * std::sqrt(scale);
  }
  out.on_surface = true;
  out.look = &k.dust;
  out.scale = scale;
  out.seed = h.seed;
  out.born_s = h.born_s;
  return true;
}

size_t impact_births_max(const ImpactLook& k, const ImpactRound& r) {
  return size_t{scaled(k.sparks, r.scale, r.spark_share)} + scaled(k.chips, r.scale) +
         scaled(k.grit, r.scale) + scaled(k.drops, r.scale);
}

size_t impact_births(const Impact& h, std::span<Spark> out) {
  const ImpactLook& k = look_of(h);
  const ImpactRound& r = round_of(h);
  core::Rng rng = core::make_rng(h.seed ^ kThrowSalt);
  const double root = std::sqrt(r.scale);
  double axis[3], d[3];
  impact_axis(h, k.ricochet, axis);
  size_t n = 0;
  const auto put = [&](Spark s, double speed) {
    if (n >= out.size()) return;
    for (int j = 0; j < 3; ++j) {
      s.p[j] = h.at[j] + h.normal[j] * 0.01;
      s.v[j] = d[j] * speed;
    }
    out[n++] = s;
  };
  for (uint32_t i = 0, m = scaled(k.sparks, r.scale, r.spark_share); i < m; ++i) {
    in_cone(rng, axis, h.normal, k.spark_spread, d);
    const double speed = k.spark_speed * root * (0.35 + 0.65 * core::next(rng));
    put(Spark{SparkKind::spark,
              {},
              {},
              k.spark_life_s * core::range(rng, 0.5, 1.5),
              0.01,
              k.spark_k + core::range(rng, -200, 200),
              core::range(rng, 0.1, 0.2)},
        speed);
  }
  for (uint32_t i = 0, m = scaled(k.chips, r.scale); i < m; ++i) {
    in_cone(rng, axis, h.normal, k.chip_spread, d);
    const double speed = k.chip_speed * root * (0.3 + 0.7 * core::next(rng));
    put(Spark{SparkKind::chip,
              {},
              {},
              k.chip_life_s * core::range(rng, 0.6, 1.4),
              k.chip_size_m * root * core::range(rng, 0.6, 1.4),
              0,
              1,
              std::clamp(k.chip_albedo + core::range(rng, -0.06, 0.06), 0.0, 1.0)},
        speed);
  }
  for (uint32_t i = 0, m = scaled(k.grit, r.scale); i < m; ++i) {
    in_cone(rng, axis, h.normal, 1.1, d);
    const double speed = k.grit_speed * root * (0.3 + 0.7 * core::next(rng));
    put(
        Spark{
            SparkKind::grit, {}, {}, core::range(rng, 0.6, 1.4), 0.009 * root, 0, 1, k.grit_albedo},
        speed);
  }
  for (uint32_t i = 0, m = scaled(k.drops, r.scale); i < m; ++i) {
    in_cone(rng, axis, h.normal, k.drop_spread, d);
    const double speed = k.drop_speed * root * (0.35 + 0.65 * std::sqrt(core::next(rng)));
    put(Spark{SparkKind::drop, {}, {}, core::range(rng, 0.5, 1.0), 0.016 * root, 0, 1, -1}, speed);
  }
  return n;
}

}  // namespace ge::render
