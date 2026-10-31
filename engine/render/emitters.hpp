// EMITTERS AN AREA CARRIES (docs/rendering.md § Particles and figures, M38b): what an area of the
// world gives off on its own, as births for render/particles.hpp's pool: EMBERS rising off a glow
// (a fire's, a lava pool's haze), MOTES drifting in a lit dust, DRIPS falling from a ceiling or an
// icicle, STEAM lifting off warm water.
//
// - THE AREA is a box (`centre`, `half`, world m): embers and steam are born over its floor, drips
//   under its ceiling, motes all through it.
// - THE CADENCE: births come at `rate` a second, the k-th in its slot [k, k + 1) / rate (a drip on
//   the slot's start, the others anywhere in it), so the births in any span of time are a pure
//   function of the emitter and the span: a replay, a lab and a paused frame see the same ones,
//   and two frames' spans that meet give each birth once. Each is flown (`step_particle`) from
//   its moment to the span's end (in sixtieths of a second, as the pool flies it), so a slow
//   frame's births are spread along their paths.
// - EACH BIRTH is drawn off its own stream (the seed and its number): where in the area, its
//   speed, its life, and for an ember its heat (1300 to 1700 K, cooling as it rises: it reddens
//   and goes out). A SPARK is thrown up out of the fuel bed fast (1.5 to 4 m/s), hotter (2900 to
//   3400 K, from the hottest of the bed, cooling fast) and lofted by the fire's plume while it
//   slows; it is born moving with the air at the emitter (`air`, m/s), so a wind carries a fire's
//   sparks off downwind as they climb. The pool lights them as it lights every particle (the sun,
//   the sky, the lamps round them, the fog's haze), so an ember over a fire is lit by the fire and
//   glows on its own.
// - `embers_over` gives a glowing fog volume (render/fog.hpp) its embers: over its floor, as many
//   as its glow and size give.
//
// ge:summary Emitters an area carries: embers, motes, drips and steam as seeded particle births
// ge:keywords render particles emitter embers sparks motes dust drips ceiling icicle steam
// ge:keywords area-emitter emit_area embers_over births cadence seeded bonfire fire-sparks
// ge:skill render
// ge:test tests/render_emitters_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "render/fog.hpp"
#include "render/particles.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxAreaBirths = 4096;  // a span's births, at most (a long pause's)

struct AreaEmitter {
  enum class Kind : uint8_t { embers, motes, drips, steam, sparks };
  double centre[3] = {0, 0, 0};  // world m
  double half[3] = {1, 1, 1};    // half its size along x, y, z, m
  double rate = 10;              // births a second
  uint32_t seed = 0;
  Kind kind = Kind::embers;
  double air[3] = {0, 0, 0};  // m/s: the air at the area, which a spark is born moving with
};

// The births of `emitter` in [t0_s, t1_s), each flown to t1_s, appended to `out` (at most
// kMaxAreaBirths; the latest kept).
void emit_area(const AreaEmitter& emitter, double t0_s, double t1_s, std::vector<Particle>& out);

// The embers rising off a glowing fog volume, standing on `floor_y` when it hugs the ground: over
// the middle of its floor, a birth a second for every candela of its lamp's (`fog_glow_light`)
// over ten, at least 4 and at most 60.
AreaEmitter embers_over(const FogVolume& volume, double floor_y = 0, uint32_t seed = 0);

}  // namespace ge::render
