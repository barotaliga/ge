// A BODY BURNED BY A HOT LIQUID OR EATEN BY AN ACID ONE (docs/liquids.md § Bodies in it): what a
// figure that wades into lava or slime shows, and how it wears off once it is out. Presentation
// only: a game keeps one `BodyBurn` a body, steps it every frame by how deep the body is in which
// liquid, and draws what it says; nothing here feeds a simulation.
//
// - THE STATE (`BodyBurn`, `step_body_burn`): four levels, 0..1, each a first-order lag toward a
// target.
//   HEAT is the skin's heat: up quickly while the body is in a hot liquid (the deeper, the hotter),
//   down over a second or two once it is out. CHARRED is the soot and the burned skin: it builds
//   while the heat is high and wears off at an even pace once the heat is under it, a whole char
//   in four seconds. ACID is the wet acid on the skin (up in a slime, down once out), ETCHED the
//   bleached, eaten skin it leaves, wearing off the same way. `BodyBurnLook` holds the times.
// - WHAT IT THROWS (`body_burn_births`): smoke off a hot body (dark and thick while it burns,
// thinner
//   as it cools), small flames here and there (a handful of spots on the body, fixed by its seed,
//   the weakest going out first as it cools), a few embers; off an acid-wet one, pale
//   yellow-green fumes. Particles for render/particles.hpp's pool, at a rate by the level, seeded
//   from the body's own stream so the same run throws the same.
// - WHAT ITS SKIN SHOWS (`body_burn_skin`): the char, the embers glowing in its cracks and the
// etch,
//   for render/morph.hpp's `MorphDraw::burn`.
// - WHAT THE AIR ROUND IT DOES (`body_burn_stir`): how much hotter and smokier the air over the
// liquid
//   is round the body (render/liquid.hpp's `LiquidStir`): a plume of shimmer and smoke out of the
//   lava round a burning body, of fumes out of the slime round an acid-wet one.
//
// ge:summary A body burned by lava or eaten by slime: heat, char, acid, its smoke, flames and fumes
// ge:keywords render scorch burn burning charred char soot smoke fire flames embers acid fumes
// ge:keywords corrode etch lava slime step_body_burn body_burn_births body_burn_skin body_burn_stir
// ge:keywords body_burn_look body-in-lava body-in-slime presentation
// ge:skill render water
// ge:test tests/render_body_burn_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "render/particles.hpp"

namespace ge::render {

struct BodyBurnLook {
  double heat_in_s = 0.35;  // s: heat's e-folding up, all in a hot liquid
  double heat_out_s = 0.9;  // s: its e-folding down once out
  double char_in_s = 0.5;   // s: the char builds at this, times the heat
  double char_out_s = 4;    // s: a whole char wears off in this once the heat is under it
  double acid_in_s = 0.4;   // s: the acid wets the skin
  double acid_out_s = 1.0;  // s: and drips off
  double etch_in_s = 0.9;   // s: the etch builds, times the acid
  double etch_out_s = 3.5;  // s: a whole etch wears off in this once the acid is under it
  double smoke_per_s = 34;  // puffs a second off a body at full heat (or charred, a third)
  double flame_per_s = 70;  // licks a second at full heat, over every spot together
  double ember_per_s = 6;   // embers a second at full heat
  double fume_per_s = 18;   // fumes a second off an acid-wet body
  double flame_k = 1500;    // K: a lick's heat when it leaves the skin
};

// The look the shooters' bodies use: a few seconds to wear off after leaving.
const BodyBurnLook& body_burn_look();

struct BodyBurn {
  float heat = 0, charred = 0, acid = 0, etched = 0;  // 0..1
  uint32_t seed = 0;                                  // the body's own: its spots, its stream
  core::Rng rng{0};                                   // the births' stream (from `seed`)
  double debt[4] = {0, 0, 0, 0};                      // births owed: smoke, flames, embers, fumes
};

// A body's state from its own seed.
BodyBurn make_body_burn(uint32_t seed);

// One frame of `dt` s with `hot` (0..1: how much of the body is in a hot liquid, 1/3 its feet, 2/3
// its waist, 1 all in) and `acid` (the same in an acid liquid).
void step_body_burn(BodyBurn& s, const BodyBurnLook& look, double hot, double acid, double dt);
// Whether it shows nothing and throws nothing (a game drops it then).
bool body_burn_cold(const BodyBurn& s);

// The body as `body_burn_births` reads it, world metres, y up.
struct BurnShape {
  double feet[3] = {0, 0, 0};  // the middle of its feet
  double height_m = 1.4;
  double radius_m = 0.3;
  double heading = 0;       // rad, the way it faces (its spots turn with it)
  double v[3] = {0, 0, 0};  // m/s, its own motion (the smoke leaves with some of it)
  double level_y = -1e9;    // the liquid's level (no flame lit under it), m
};

// This frame's smoke, flames, embers and fumes off `body` (`dt` s) appended to `out`; the count.
size_t body_burn_births(BodyBurn& s, const BodyBurnLook& look, const BurnShape& body, double dt,
                        std::vector<Particle>& out);

// What its skin shows (render/morph.hpp's MorphBurn), each 0..1.
struct BurnSkin {
  float charred = 0;  // soot and burned skin, in patches that spread as it grows
  float embers = 0;   // the heat glowing in the char's cracks
  float etched = 0;   // the acid's bleached, eaten skin
  float seed = 0;     // the patches' offset, 0..1
};
BurnSkin body_burn_skin(const BodyBurn& s);

// How much the air round it is stirred: `heat` the shimmer's share and `smoke` the smoke's or the
// fumes' (both 0..1), for a LiquidStir.
struct BurnStir {
  float heat = 0, smoke = 0;
};
BurnStir body_burn_stir(const BodyBurn& s);

}  // namespace ge::render
