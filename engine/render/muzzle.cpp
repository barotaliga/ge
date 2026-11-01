// The muzzle presets and the knobs' table, the flame on a pose, the puff, the flash, the sparks.
//
// ge:summary Muzzle fire definitions: the weapons' presets, the knobs, the flame's curves, births
// ge:keywords render muzzle muzzle-flash flame smoke sparks presets knobs shotgun nailgun rocket
// ge:skill render
// ge:test tests/render_muzzle_test.cpp
#include "render/muzzle.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "render/blackbody.hpp"

namespace ge::render {

namespace {

namespace lm = ge::core::libm;
constexpr double kPi = 3.14159265358979323846;

// The sparks' stream and the prongs' turn, each its own.
constexpr uint32_t kSparkSalt = 0x51ed27a3u;
constexpr uint32_t kTurnSalt = 0x2545f491u;

// The presets: each weapon's fire, and the smoke its powder or its motor leaves.
constexpr MuzzleLook kShotgun{.id = "muzzle.shotgun",
                              .length_m = 0.4,
                              .width_m = 0.24,
                              .life_s = 0.06,
                              .core_k = 1950,
                              .flash = 0.5,
                              .prongs = 5,
                              .prong_depth = 0.6,
                              .billows = 0.6,
                              .fray = 0.5,
                              .smoke = {.id = "muzzle.shotgun.smoke",
                                        .radius_m = 0.16,
                                        .grow_s = 0.05,
                                        .smoke_grow = 1.5,
                                        .rise = 0.5,
                                        .fire = 0,
                                        .flash = 0,
                                        .smoke = 1.0,
                                        .smoke_in_s = 0.04,
                                        .smoke_s = 1.6,
                                        .smoke_albedo = 0.5,
                                        .smoke_tint = {0.92f, 0.9f, 0.86f},
                                        .billows = 0.7,
                                        .fray = 0.45,
                                        .billow_size = 1.3,
                                        .roll = 0.6,
                                        .sparks = 0,
                                        .debris = 0,
                                        .scorch_m = 0,
                                        .light_range_m = 0},
                              .smoke_speed = 4,
                              .sparks = 10,
                              .spark_speed = 25,
                              .spark_life_s = 0.18,
                              .spark_k = 2100,
                              .spread = 0.18,
                              .light_range_m = 9};
constexpr MuzzleLook kNailgun{.id = "muzzle.nailgun",
                              .length_m = 0.16,
                              .width_m = 0.07,
                              .life_s = 0.03,
                              .core_k = 2150,
                              .flash = 0.3,
                              .prongs = 0,
                              .billows = 0.3,
                              .fray = 0.3,
                              .billow_size = 1.6,
                              .smoke = {.id = "muzzle.nailgun.smoke",
                                        .radius_m = 0.06,
                                        .grow_s = 0.04,
                                        .smoke_grow = 1.2,
                                        .rise = 0.6,
                                        .fire = 0,
                                        .flash = 0,
                                        .smoke = 0.35,
                                        .smoke_in_s = 0.03,
                                        .smoke_s = 0.6,
                                        .smoke_albedo = 0.7,
                                        .smoke_tint = {0.95f, 0.96f, 0.98f},
                                        .billows = 0.5,
                                        .fray = 0.4,
                                        .billow_size = 1.3,
                                        .roll = 0.8,
                                        .sparks = 0,
                                        .debris = 0,
                                        .scorch_m = 0,
                                        .light_range_m = 0},
                              .smoke_speed = 2,
                              .sparks = 3,
                              .spark_speed = 14,
                              .spark_life_s = 0.12,
                              .spark_k = 2500,
                              .spread = 0.3,
                              .light_range_m = 5};
constexpr MuzzleLook kGrenade{.id = "muzzle.grenade",
                              .length_m = 0.2,
                              .width_m = 0.17,
                              .life_s = 0.07,
                              .core_k = 1650,
                              .cool_k = 1200,
                              .flash = 0.3,
                              .prongs = 0,
                              .billows = 0.9,
                              .fray = 0.5,
                              .billow_size = 1.1,
                              .smoke = {.id = "muzzle.grenade.smoke",
                                        .radius_m = 0.2,
                                        .grow_s = 0.06,
                                        .smoke_grow = 1.4,
                                        .rise = 0.3,
                                        .fire = 0,
                                        .flash = 0,
                                        .smoke = 1.2,
                                        .smoke_in_s = 0.05,
                                        .smoke_s = 2.0,
                                        .smoke_albedo = 0.35,
                                        .smoke_tint = {0.85f, 0.78f, 0.7f},
                                        .billows = 0.8,
                                        .fray = 0.4,
                                        .billow_size = 1.2,
                                        .roll = 0.5,
                                        .sparks = 0,
                                        .debris = 0,
                                        .scorch_m = 0,
                                        .light_range_m = 0},
                              .smoke_speed = 2.5,
                              .sparks = 2,
                              .spark_speed = 8,
                              .spark_life_s = 0.25,
                              .spark_k = 1900,
                              .spread = 0.35,
                              .light_range_m = 6};
constexpr MuzzleLook kRocket{.id = "muzzle.rocket",
                             .length_m = 0.9,
                             .width_m = 0.2,
                             .life_s = 0.12,
                             .core_k = 2250,
                             .cool_k = 1500,
                             .glow = 0.09,
                             .flash = 0.55,
                             .prongs = 0,
                             .billows = 0.7,
                             .fray = 0.35,
                             .billow_size = 1.6,
                             .smoke = {.id = "muzzle.rocket.smoke",
                                       .radius_m = 0.25,
                                       .grow_s = 0.06,
                                       .smoke_grow = 1.6,
                                       .rise = 0.4,
                                       .fire = 0,
                                       .flash = 0,
                                       .smoke = 1.4,
                                       .smoke_in_s = 0.05,
                                       .smoke_s = 2.6,
                                       .smoke_albedo = 0.6,
                                       .smoke_tint = {0.95f, 0.95f, 0.95f},
                                       .billows = 0.7,
                                       .fray = 0.4,
                                       .billow_size = 1.2,
                                       .roll = 0.5,
                                       .sparks = 0,
                                       .debris = 0,
                                       .scorch_m = 0,
                                       .light_range_m = 0},
                             .smoke_speed = 6,
                             .sparks = 14,
                             .spark_speed = 30,
                             .spark_life_s = 0.3,
                             .spark_k = 2500,
                             .spread = 0.12,
                             .light_range_m = 14};
constexpr std::array<MuzzleLook, 4> kLooks{kShotgun, kNailgun, kGrenade, kRocket};

constexpr std::array<MuzzleKnob, 19> kKnobs{{
    {"length", "m", "the flame's length along the barrel", &MuzzleLook::length_m, nullptr, 0.02, 3},
    {"width", "m", "the flame's width across the barrel", &MuzzleLook::width_m, nullptr, 0.02, 2},
    {"life", "s", "how long the flame lasts", &MuzzleLook::life_s, nullptr, 0.01, 0.5},
    {"heat", "K", "the flame's heat as it leaves the barrel", &MuzzleLook::core_k, nullptr, 900,
     3200},
    {"cool_to", "K", "the heat it ends at", &MuzzleLook::cool_k, nullptr, 700, 2500},
    {"glow", "", "how bright the flame looks, a share of a blackbody's", &MuzzleLook::glow, nullptr,
     0.005, 1},
    {"flash", "", "how bright its light is, a share of a blackbody's", &MuzzleLook::flash, nullptr,
     0, 1},
    {"prongs", "", "petals round the barrel's axis (0: a round jet)", nullptr, &MuzzleLook::prongs,
     0, 8},
    {"prong_depth", "", "how deep the petals cut", &MuzzleLook::prong_depth, nullptr, 0, 1},
    {"billows", "", "how deep the flame's billows cut", &MuzzleLook::billows, nullptr, 0, 1.5},
    {"fray", "", "the fine fraying at its edge", &MuzzleLook::fray, nullptr, 0, 1},
    {"billow_size", "", "the billows' size against the flame's width", &MuzzleLook::billow_size,
     nullptr, 0.4, 3},
    {"smoke_speed", "m/s", "how fast the puff leaves the muzzle", &MuzzleLook::smoke_speed, nullptr,
     0, 30},
    {"sparks", "", "sparks a shot throws", nullptr, &MuzzleLook::sparks, 0, 100},
    {"spark_speed", "m/s", "the fastest spark", &MuzzleLook::spark_speed, nullptr, 1, 120},
    {"spark_life", "s", "a spark's mean life", &MuzzleLook::spark_life_s, nullptr, 0.02, 3},
    {"spark_heat", "K", "a spark's heat at birth", &MuzzleLook::spark_k, nullptr, 900, 3200},
    {"spread", "rad", "the sparks' cone, its half angle", &MuzzleLook::spread, nullptr, 0, 1.5},
    {"light_range", "m", "where the flash's light ends", &MuzzleLook::light_range_m, nullptr, 0,
     100},
}};

double smoothstep(double a, double b, double x) {
  const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

void cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

// The flame's length and width at `u`, a share of its life: it shoots out to its length in a
// third of its life, then holds while it burns out.
void size_at(const MuzzleLook& k, double u, double& length, double& width) {
  const double grown = std::sqrt(std::min(1.0, u / 0.33));
  length = k.length_m * (0.45 + 0.55 * grown);
  width = k.width_m * (0.6 + 0.4 * grown);
}

}  // namespace

std::span<const MuzzleLook> muzzle_looks() { return kLooks; }

const MuzzleLook& default_muzzle() { return kLooks[0]; }

const MuzzleLook* muzzle_look_by_id(std::string_view id) {
  for (const MuzzleLook& l : kLooks) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

std::span<const MuzzleKnob> muzzle_knobs() { return kKnobs; }

bool set_muzzle_knob(MuzzleLook& look, std::string_view name, double value) {
  constexpr std::string_view kSmoke = "smoke.";
  if (name.substr(0, kSmoke.size()) == kSmoke) {
    return set_knob(look.smoke, name.substr(kSmoke.size()), value);
  }
  for (const MuzzleKnob& k : kKnobs) {
    if (k.name != name) continue;
    const double v = std::clamp(value, k.lo, k.hi);
    if (k.number) look.*k.number = v;
    if (k.count) look.*k.count = static_cast<uint32_t>(std::lround(v));
    return true;
  }
  return false;
}

bool tuned_muzzle(std::string_view preset, std::string_view knobs, MuzzleLook* out) {
  const MuzzleLook* base = muzzle_look_by_id(preset);
  if (!base) return false;
  *out = *base;
  for (std::string_view rest = knobs; !rest.empty();) {
    const size_t comma = rest.find(',');
    const std::string_view one = rest.substr(0, comma);
    rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    const size_t eq = one.find('=');
    if (eq == std::string_view::npos) return false;
    const std::string text(one.substr(eq + 1));
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (text.empty() || end != text.c_str() + text.size()) return false;
    if (!set_muzzle_knob(*out, one.substr(0, eq), v)) return false;
  }
  return true;
}

void scale_muzzle(MuzzleLook& look, double s) {
  look.length_m *= s;
  look.width_m *= s;
  look.smoke.radius_m *= s;
  look.smoke_speed *= s;
  look.spark_speed *= s;
}

Flame flame_at(const Shot& s, const MuzzlePose& pose, double now_s) {
  const MuzzleLook& k = look_of(s);
  const double t = now_s - s.born_s, u = std::clamp(t / k.life_s, 0.0, 1.0);
  Flame f{};
  f.age_s = t;
  f.alive = t >= 0 && t < k.life_s;
  double length = 0, width = 0;
  size_at(k, u, length, width);
  for (int i = 0; i < 3; ++i) {
    f.axis[i] = pose.dir[i];
    f.centre[i] = pose.at[i] + pose.dir[i] * 0.5 * length;
  }
  f.radius_m = 0.5 * width;
  f.stretch = std::max(1.0, length / width);
  f.fire = 1 - smoothstep(0.15, 1, u);
  f.core_k = k.cool_k + (k.core_k - k.cool_k) * (1 - u);
  // The first prong's way: the gun's up turned about the barrel by the shot's own share of a
  // petal, so one weapon's shots do not all flower alike.
  core::Rng rng = core::make_rng(s.seed ^ kTurnSalt);
  const double turn = k.prongs > 0 ? core::range(rng, 0, 2 * kPi / k.prongs) : 0.0;
  double across[3];
  cross(pose.dir, pose.up, across);
  const double c = lm::cos(turn), sn = lm::sin(turn);
  for (int i = 0; i < 3; ++i) f.side[i] = pose.up[i] * c + across[i] * sn;
  return f;
}

Blast smoke_of(const Shot& s, const MuzzlePose& pose_at_shot) {
  const MuzzleLook& k = look_of(s);
  Blast b;
  for (int i = 0; i < 3; ++i) {
    b.at[i] = pose_at_shot.at[i] + pose_at_shot.dir[i] * 0.3 * k.length_m;
    b.normal[i] = pose_at_shot.dir[i];
    b.v[i] = pose_at_shot.dir[i] * k.smoke_speed;
  }
  b.look = &k.smoke;
  b.seed = s.seed;
  b.born_s = s.born_s;
  return b;
}

Flash muzzle_flash_at(const Shot& s, const MuzzlePose& pose, double now_s) {
  const MuzzleLook& k = look_of(s);
  const Flame f = flame_at(s, pose, now_s);
  Flash out{};
  for (int i = 0; i < 3; ++i) out.at[i] = f.centre[i];
  const Blackbody c = blackbody(f.core_k);
  for (int i = 0; i < 3; ++i) out.colour[i] = c.rgb[i];
  const double width = 2 * f.radius_m, length = width * f.stretch;
  out.candela = f.alive ? length * width * blackbody_cd(f.core_k) * f.fire * k.flash : 0;
  out.range_m = k.light_range_m;
  out.radius_m = std::max(f.radius_m, 0.02);
  return out;
}

size_t shot_births_max(const MuzzleLook& look) { return look.sparks; }

size_t shot_births(const Shot& s, const MuzzlePose& pose_at_shot, std::span<Spark> out) {
  const MuzzleLook& k = look_of(s);
  core::Rng rng = core::make_rng(s.seed ^ kSparkSalt);
  const double* dir = pose_at_shot.dir;
  double across[3];
  cross(dir, pose_at_shot.up, across);
  size_t n = 0;
  for (uint32_t i = 0; i < k.sparks && n < out.size(); ++i) {
    // Anywhere in the cone, more of them near its axis.
    const double off = k.spread * std::sqrt(core::next(rng)), round = core::range(rng, 0, 2 * kPi);
    const double ca = lm::cos(off), sa = lm::sin(off), cr = lm::cos(round), sr = lm::sin(round);
    const double speed = k.spark_speed * (0.4 + 0.6 * core::next(rng));
    Spark sp{SparkKind::spark,
             {},
             {},
             k.spark_life_s * core::range(rng, 0.5, 1.5),
             0.012,
             k.spark_k + core::range(rng, -200, 200),
             core::range(rng, 0.12, 0.25)};
    for (int j = 0; j < 3; ++j) {
      const double d = dir[j] * ca + (pose_at_shot.up[j] * cr + across[j] * sr) * sa;
      sp.p[j] = pose_at_shot.at[j] + d * 0.02;
      sp.v[j] = d * speed;
    }
    out[n++] = sp;
  }
  return n;
}

}  // namespace ge::render
