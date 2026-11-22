// A HOT MARK BURNS A LITTLE, THEN SMOKES (docs/detail.md § Decals): what a glowing mark on a wall
// does while it cools. A beam's burnt line and its melt, a slug's or an energy ball's glowing mark
// (engine/detail's `burn_line` and `melt`, render/marks.hpp's `press_glow`) burn in a few small
// flames for the first part of their glow; the flames shrink and go out one by one as the mark
// cools, and thin grey smoke rises off it instead, thickest as the last of the glow goes and gone a
// little after. Presentation only, and meant to be subtle: a few small tongues and wisps, never a
// fire.
//
// - THE ENVELOPE (`mark_fire_level`): by the share of its glow a mark has lived, x = age / glow:
//   the flames at once, dying down to nothing by `fire_until` (x); the smoke rising from
//   `smoke_from` to its most at `smoke_peak`, falling to nothing by `smoke_until` (past 1: after
//   the glow is gone).
// - THE FLAMES ARE BURNING GAS (`mark_flames`): render/flame.hpp's marched flames, so they lick,
//   puff and snake as a torch's do, a few centimetres tall. Each mark burns in one flame for every
//   `flame_area_m2` of it (at most `flames_per_mark`), each on its own spot where the mask glows,
//   fixed by the mark's serial, so a flame stays where it was lit. Each spot has its own strength:
//   the weak ones are shorter and go out first as the mark cools, the strongest last; a flame's
//   height and heat fall with what is left of its fire. A mark facing down (a ceiling) burns in
//   none: a flame stands up off its fuel. A pure function of the pool and the time.
// - THE SMOKE IS PARTICLES (`mark_fire_births`): wisps from a point on one of the mark's own
//   triangles where its mask covers (so a mark wrapped over an edge smokes on both faces), a
//   centimetre out of the surface, rising and spreading as they thin. A rate a second for a mark of
//   `area_m2`, in proportion to its area (a beam's line of pieces smokes as much as one mark of
//   their area), capped at `area_cap` times it, and at `most` births a frame over every mark. A
//   frame longer than `max_dt_s` (a seek, a stall) throws nothing. The births come from the state's
//   own stream and each slot's carried debt, so the same frames throw the same.
//
// ge:summary A hot mark's flames, then its smoke as it cools: gas flames and wisps off the decals
// ge:keywords render marks decals burn smoulder smoke flames fire cooling melt burn-line glow
// ge:keywords mark_fire_births mark_flames mark_fire_level mark_fire_look smoulders presentation
// ge:skill render detail
// ge:test tests/render_mark_fire_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "detail/decals.hpp"
#include "render/flame.hpp"
#include "render/particles.hpp"

namespace ge::render {

struct MarkFireLook {
  double fire_until = 0.7;   // the flames are out by this share of the glow
  double smoke_from = 0.3;   // the smoke starts at this share
  double smoke_peak = 0.75;  // is thickest here
  double smoke_until = 1.6;  // and is gone by this share (past 1: after the glow)
  // The flames.
  double flame_area_m2 = 0.035;  // a flame for every this much of a mark
  int flames_per_mark = 5;       // and at most this many on one
  int most_flames = 48;          // over every mark
  double flame_height_m = 0.12;  // the strongest flame's height on a fresh mark of `area_m2`
  double flame_tallest_m = 0.3;  // a big mark's (its flames grow with the root of its area)
  double flame_k = 1800;         // the hottest gas of a fresh mark's flame, K
  double flame_cool_k = 1350;    // and of one about to go out
  // The smoke.
  double area_m2 = 0.1;         // the mark the smoke's rate is for (a slug's is ~0.08)
  double area_cap = 4;          // a bigger mark throws at most this many times the rate
  double smoke_per_s = 6;       // wisps a second off one at the smoke's most
  double smoke_albedo = 0.3;    // the smoke's grey
  double smoke_opacity = 0.15;  // a wisp at its densest
  int most = 24;                // births a frame over every mark
  double max_dt_s = 0.25;       // a longer frame throws nothing
};
const MarkFireLook& mark_fire_look();

// The kinds that burn and smoke as they cool: a beam's burnt line and a melt.
bool smoulders(detail::DecalKind kind);

// The flames' and the smoke's levels (0..1) of a mark `age_s` into a glow of `glow_s`.
struct MarkFireLevel {
  double fire = 0, smoke = 0;
};
MarkFireLevel mark_fire_level(const MarkFireLook& look, double age_s, double glow_s);

// The flames burning on every smouldering mark of `pool` at `now_s` appended to `out` (each with
// its own flicker; `relative` 0: a caller lighting its scene in units of its own sets it); the
// count.
size_t mark_flames(const MarkFireLook& look, const detail::DecalPool& pool, double now_s,
                   std::vector<Flame>& out);

struct MarkFire {
  core::Rng rng{0x6d61726bu};
  std::vector<uint32_t> serial;  // each slot's decal its debt is for
  std::vector<double> debt;      // smoke owed, a slot each
};

// This frame's smoke off every smouldering mark of `pool` at `now_s` (`dt` s since the last frame)
// appended to `out`; the count.
size_t mark_fire_births(MarkFire& s, const MarkFireLook& look, const detail::DecalPool& pool,
                        double now_s, double dt, std::vector<Particle>& out);

}  // namespace ge::render
