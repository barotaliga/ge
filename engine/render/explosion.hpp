// AN EXPLOSION, AS SEEN (docs/rendering.md § Explosions): what a blast looks like at each moment
// of its life, as pure functions of the blast and its age, so a replay, a lab and two clients
// see the same fireball. A game says where and when something went off (a `Blast`); this file
// says the rest, and render/explosions.hpp draws it.
//
// - THE LOOK (`BlastLook`) is every knob of one kind of blast: how big the fireball swells and
//   how fast, how much fire and for how long, how hot it starts and how it cools, how much smoke,
//   of what colour and for how long, how its billows are shaped, what it throws and the scorch
//   and light it leaves. One code draws every blast; a grenade, a rocket and another game's
//   blast are different looks. The engine's presets (`blast_looks`, each with a stable id a
//   content table names it by: "explosion", "explosion.rocket", "explosion.grenade",
//   "explosion.small", "explosion.large") are starting points: a game copies one, sets knobs
//   by name (`set_knob`, each clamped to its range, the table `blast_knobs` names and explains
//   them) and points its blasts at its own. A blast's `scale` sizes any look (lengths by it,
//   counts by its square, times by its square root).
// - THE FIREBALL (`fireball_at`) is one ball of hot gas turning into smoke: it swells to its
//   radius in a few `grow_s` (r = R (1 - e^(-t/grow))^(1/2)), then keeps growing as smoke (by
//   `smoke_grow` of R); it rises as hot air does (by `rise` of R a second once going); its core
//   cools from `core_k` toward `cool_k` (T = cool + (core - cool) e^(-t/cool_s)); its FIRE share
//   (`fire` at its start) falls as e^(-t/fire_s), while its SMOKE share (`smoke`) climbs over
//   `smoke_in_s` and thins away by `smoke_s`. Off a surface it starts a third of its radius out
//   along the surface's normal, a hemisphere rather than a ball buried in the wall. A blast thrown
//   (`v`, a muzzle's puff) is carried on at its speed, slowing as e^(-t/kBlastDragS).
// - THE FLASH (`flash_at`) is the light the fireball gives its surroundings: a point at its
//   centre whose luminous intensity is the glowing ball's (its disc, pi r^2, times its core's
//   blackbody luminance times the fire share times `flash`), in the core's blackbody colour,
//   reaching `light_range_m`. So the flash is white-hot for a few hundredths of a second and an
//   orange glow a moment later, and a game hands it to the local lights every frame.
// - WHAT IT THROWS (`blast_births`, all at the moment it goes off, dealt off the blast's seed):
//   SPARKS (white-hot specks flung out fast, cooling to red as they fall) and DEBRIS (dark
//   chunks on ballistic arcs, `debris_hot` of them still glowing); off a surface, into the half
//   space it faces. Each is a `Spark` a renderer turns into its particles.
// - THE SCORCH (`scorch_of`): a blast off a surface leaves a burnt mark there, `scorch_m` across
//   times its scale, turned and picked off the seed, for a game to press with its decals
//   (engine/detail's DecalKind::scorch, `now_s` the blast's time). A blast in the open air
//   leaves none; a game that wants one under an air burst passes the ground it traced.
//
// Pure: the same blast and age give the same bits (ge::core::libm, ge::core::Rng).
//
// ge:summary An explosion as seen: the looks and their knobs, the fireball, flash, sparks, scorch
// ge:keywords render explosion blast fireball smoke flash sparks debris scorch grenade
// ge:keywords rocket effect sprite-replacement blast_looks blast_look_by_id blast_knobs set_knob
// ge:keywords knobs preset fireball_at flash_at blast_births scorch_of blast_life
// ge:skill render
// ge:test tests/render_explosion_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace ge::render {

// Every knob of one kind of blast, at scale 1. The defaults are the common blast's.
struct BlastLook {
  std::string_view id = "explosion";  // the stable name a content table uses
  // The fireball.
  double radius_m = 1.4;     // swollen, m
  double grow_s = 0.07;      // the swell's time constant, s
  double smoke_grow = 0.45;  // how much more it grows as smoke, a share of the radius
  double rise = 0.9;         // how fast it rises once going, radii a second
  // The fire.
  double fire = 1;       // how much of the ball burns at the start, 0..1
  double fire_s = 0.5;   // the fire's e-folding time, s
  double core_k = 2300;  // the core's heat as it goes off, K
  double cool_k = 1300;  // the heat it cools toward, K
  double cool_s = 0.5;   // the heat's e-folding time, s
  double flash = 0.5;    // the ball's glow as a share of a blackbody's (the flash's light)
  // The smoke.
  double smoke = 1;            // how thick, 0..2
  double smoke_in_s = 0.35;    // the time it takes to build, s
  double smoke_s = 3.6;        // until the last of it is gone, s (the blast's life)
  double smoke_albedo = 0.12;  // its single-scattering albedo: soot 0.1, dust 0.4, steam 0.9
  float smoke_tint[3] = {1.0f, 0.92f, 0.82f};  // the colour of the light it scatters
  // The shape.
  double billows = 0.7;    // how deep its big billows cut, 0..1.5
  double fray = 0.25;      // the fine fraying at its edge, 0..1
  double billow_size = 1;  // the billows' size against the ball's, 0.4..3
  double roll = 0.4;       // how fast it turns over, radii a second
  // What it throws.
  uint32_t sparks = 48;       // at scale 1
  double spark_speed = 22;    // the fastest, m/s
  double spark_life_s = 0.9;  // the mean, s
  double spark_k = 2350;      // their heat at birth, K
  uint32_t debris = 12;
  double debris_hot = 0.4;  // the share still glowing, 0..1
  // What it leaves.
  double scorch_m = 1.6;      // the mark's size across, m (0: none)
  double light_range_m = 18;  // where the flash's light ends, m
};

// The engine's presets.
std::span<const BlastLook> blast_looks();
const BlastLook& default_look();  // "explosion"
// The preset a content table names, or null.
const BlastLook* blast_look_by_id(std::string_view id);

// A knob by name: its unit, what it does, its range, and the field it sets.
struct BlastKnob {
  std::string_view name, unit, what;
  double BlastLook::*number = nullptr;  // one of these two
  uint32_t BlastLook::*count = nullptr;
  double lo, hi;
};
std::span<const BlastKnob> blast_knobs();
// Sets the knob `name` to `value`, clamped to its range (counts rounded): false for no such knob.
bool set_knob(BlastLook& look, std::string_view name, double value);

struct Blast {
  double at[3] = {0, 0, 0};         // world, m: where it went off
  double normal[3] = {0, 1, 0};     // the surface's, out of it (unit); unused in the open air
  bool on_surface = false;          // went off against a surface (a wall, the floor)
  const BlastLook* look = nullptr;  // null: the default; must outlive the blast
  double scale = 1;                 // lengths by it, counts by its square, times by its square root
  uint32_t seed = 0;                // its own sparks, scorch and the turn of its noise
  double born_s = 0;                // the clock it went off at
  double v[3] = {0, 0, 0};          // m/s it is thrown at (a muzzle's puff), slowed by the air
};
inline constexpr double kBlastDragS = 0.3;  // the air's e-folding time on a thrown blast, s
inline const BlastLook& look_of(const Blast& b) { return b.look ? *b.look : default_look(); }

// How long the blast is seen, s.
double blast_life(const Blast& b);

struct Fireball {
  double centre[3];  // world, m
  double radius_m;
  double core_k;  // the core's temperature, K
  double fire;    // the share still burning, 0..1
  double smoke;   // the smoke's density, 0..2
  double age_s;
  bool alive;  // false before it goes off and once its life is over
};
Fireball fireball_at(const Blast& b, double now_s);

struct Flash {
  double at[3];     // world, m
  float colour[3];  // linear rgb, luminance 1
  double candela;   // 0 once the fire is out
  double range_m;
  double radius_m;  // the emitter's size
};
Flash flash_at(const Blast& b, double now_s);

// What a thrown thing is, which says how the particle pool flies and shades it: a glowing SPARK, a
// dark chunk of DEBRIS, a CHIP of a surface a round hit (its `albedo` the surface's shade), a
// GRIT of fine dust that hangs and drifts, a DROP of a splash.
enum class SparkKind : uint8_t { spark, debris, chip, grit, drop };
struct Spark {
  SparkKind kind;
  double p[3];  // world, m, at the blast's moment
  double v[3];  // m/s
  double life_s;
  double size_m;
  double kelvin;       // a spark's heat at birth (0: none)
  double cool_s;       // its heat's e-folding time
  double albedo = -1;  // its whiteness, 0..1 (below 0: its kind's)
};
// Upper bound on what one blast of `look` at `scale` throws.
size_t blast_births_max(const BlastLook& look, double scale);
// What the blast throws, written into `out` (at most its size): the count written.
size_t blast_births(const Blast& b, std::span<Spark> out);

struct Scorch {
  double at[3];
  double normal[3];
  double size_m;
  double spin_rad;
  uint32_t variant;  // 0..kScorchVariants - 1
  double born_s;
};
inline constexpr uint32_t kScorchVariants = 4;
// The mark the blast leaves: false in the open air or for a look that leaves none.
bool scorch_of(const Blast& b, Scorch& out);

}  // namespace ge::render
