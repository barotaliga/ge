// A ROUND HITTING A SURFACE, AS SEEN (docs/rendering.md § Impacts): the burst a nail or a pellet
// throws off what it hits, different for what that is made of, as pure functions of the hit. It
// is the explosions' smoke and throw (render/explosion.hpp) shaped for a small, fast hit, drawn by
// the same passes (the puff by render/explosions.hpp, the rest by the particle pool).
//
// - THE LOOK (`ImpactLook`) is every knob of one surface's answer: the DUST it kicks out (a whole
//   `BlastLook` of its own, a puff of its colour and weight: grey powder off stone, tan off wood,
//   a dark plume off dirt, a white mist off water) and how fast it leaves; how much of what is
//   thrown follows the round's glance off the surface rather than the surface's normal
//   (`ricochet`: a steel spike off a plate sprays its sparks on along its line, a pellet into
//   dirt kicks straight up); SPARKS (white-hot specks, many off metal, a few off stone, none off
//   wood); CHIPS (fragments on ballistic arcs, of the surface's shade: stone flakes, splinters,
//   clods, shards of tile); GRIT (fines that hang a moment and drift down); DROPS (a splash's
//   beads). `impact_knobs` names them, `set_impact_knob` sets one by name (clamped), and a
//   "dust.<knob>" name sets the puff's own (render/explosion.hpp's `blast_knobs`).
// - THE PRESETS are named "impact.<surface>" with the words a level's surfaces are classed by
//   (engine/detail's `surface_name`): stone, dirt, grass, sand, wood, metal, tile, liquid, and
//   unknown (stone's). `impact_look_for` takes that word and gives the look; the sky gives none
//   (a round into the sky hits nothing). So a game classes the texture it hit and asks.
// - THE ROUND (`ImpactRound`) sizes the burst: its `scale` sizes the look (lengths by it, counts
//   by its square, times by its square root, as a blast's), and `spark_share` is how much of the
//   look's sparks it strikes (a steel nail all of them, a soft lead pellet few). The presets:
//   "round.nail", "round.pellet". A shotgun's shot is a handful of pellet hits spread round where
//   it was aimed, each its own seed.
// - WHERE IT GOES: an `Impact` is the point hit, the surface's normal there, the way the round
//   was going, the look and the round, a seed and the clock. `dust_of` is the puff as a `Blast`
//   on the surface, thrown out along the burst's axis; `impact_births` are its sparks, chips,
//   grit and drops as `Spark`s for the particle pool, each into the half space the surface faces.
//   The MARK the hit leaves (a hole, a dent) is the decals' (engine/detail's `shot_hit`), pressed
//   at the same point.
//
// Pure: the same hit gives the same bits (ge::core::libm, ge::core::Rng).
//
// ge:summary A round hitting a surface as seen: per-material looks, dust, sparks, chips, drops
// ge:keywords render impact hit bullet-hit ricochet nail pellet shotgun wall material surface
// ge:keywords stone metal wood dirt sand tile water liquid dust chips splinters sparks splash
// ge:keywords impact_looks impact_look_for set_impact_knob dust_of impact_births round
// ge:skill render
// ge:test tests/render_impact_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "render/explosion.hpp"

namespace ge::render {

// Every knob of one surface's answer to a hit, for a round of scale 1. The defaults are stone's.
struct ImpactLook {
  std::string_view id = "impact";
  // The dust it kicks out: a puff, a blast of its own (render/explosion.hpp's knobs).
  BlastLook dust{.id = "impact.dust",
                 .radius_m = 0.09,
                 .grow_s = 0.03,
                 .smoke_grow = 1.8,
                 .rise = 0.15,
                 .fire = 0,
                 .flash = 0,
                 .smoke = 0.9,
                 .smoke_in_s = 0.02,
                 .smoke_s = 1.2,
                 .smoke_albedo = 0.5,
                 .smoke_tint = {0.9f, 0.88f, 0.84f},
                 .billows = 0.7,
                 .fray = 0.5,
                 .billow_size = 1.2,
                 .roll = 0.5,
                 .sparks = 0,
                 .debris = 0,
                 .scorch_m = 0,
                 .light_range_m = 0};
  double dust_speed = 2.5;  // how fast the puff leaves the surface, m/s (it slows in the air)
  double ricochet = 0.3;    // the burst's axis: 0 the surface's normal, 1 the round's glance
  // White-hot specks.
  uint32_t sparks = 3;
  double spark_speed = 9;     // the fastest, m/s
  double spark_life_s = 0.2;  // the mean, s
  double spark_k = 2000;      // their heat at birth, K
  double spark_spread = 0.6;  // the cone's half angle round the axis, rad
  // Fragments of the surface.
  uint32_t chips = 6;
  double chip_speed = 5;  // the fastest, m/s
  double chip_size_m = 0.018;
  double chip_life_s = 0.7;  // the mean, s
  double chip_albedo = 0.45;
  double chip_spread = 0.9;  // the cone's half angle, rad
  // Fines that hang and drift down.
  uint32_t grit = 8;
  double grit_speed = 2.5;
  double grit_albedo = 0.5;
  // A splash's beads.
  uint32_t drops = 0;
  double drop_speed = 4;
  double drop_spread = 0.35;  // rad
};

// The engine's presets, one a surface class.
std::span<const ImpactLook> impact_looks();
const ImpactLook& default_impact();  // "impact.stone"
// The preset a content table names, or null.
const ImpactLook* impact_look_by_id(std::string_view id);
// The look for a surface class by its word ("stone", "metal", ...; engine/detail's
// `surface_name`): null for "sky", stone's for a word it does not know.
const ImpactLook* impact_look_for(std::string_view surface);

// A knob by name (the dust's are blast_knobs as "dust.<name>").
struct ImpactKnob {
  std::string_view name, unit, what;
  double ImpactLook::*number = nullptr;  // one of these two
  uint32_t ImpactLook::*count = nullptr;
  double lo, hi;
};
std::span<const ImpactKnob> impact_knobs();
// Sets the knob `name` ("dust.<knob>" for the puff's) to `value`, clamped to its range: false for
// no such knob.
bool set_impact_knob(ImpactLook& look, std::string_view name, double value);

// What hit: how big its burst is and how much it strikes sparks.
struct ImpactRound {
  std::string_view id;
  double scale;        // lengths by it, counts by its square, times by its square root
  double spark_share;  // of the look's sparks, 0..1
};
std::span<const ImpactRound> impact_rounds();
const ImpactRound& default_round();  // "round.nail"
const ImpactRound* impact_round_by_id(std::string_view id);

struct Impact {
  double at[3] = {0, 0, 0};            // world, m: the point hit
  double normal[3] = {0, 1, 0};        // the surface's there, out of it (unit)
  double dir[3] = {0, 0, 1};           // the way the round was going (unit)
  const ImpactLook* look = nullptr;    // null: the default; must outlive the hit
  const ImpactRound* round = nullptr;  // null: the default
  uint32_t seed = 0;                   // its throw and the turn of its noise
  double born_s = 0;                   // the clock it hit at
};
inline const ImpactLook& look_of(const Impact& h) { return h.look ? *h.look : default_impact(); }
inline const ImpactRound& round_of(const Impact& h) { return h.round ? *h.round : default_round(); }

// The burst's axis: the normal turned toward the round's glance off the surface by `ricochet`
// (unit, always out of the surface).
void impact_axis(const Impact& h, double ricochet, double out[3]);

// The puff the hit kicks out, on the surface: false for a look with no dust.
bool dust_of(const Impact& h, Blast& out);

// Upper bound on what one hit of `look` by `round` throws.
size_t impact_births_max(const ImpactLook& look, const ImpactRound& round);
// The sparks, chips, grit and drops the hit throws, written into `out` (at most its size): the
// count written.
size_t impact_births(const Impact& h, std::span<Spark> out);

}  // namespace ge::render
