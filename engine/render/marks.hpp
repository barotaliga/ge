// THE MARKS DRAWN (docs/detail.md § Decals, docs/rendering.md § Explosions): engine/detail's decals
// (a blast's scorch, a bullet hole, blood) drawn as the beams pass's burns (render/beams.hpp), so a
// game that presses marks with the decals draws them with no pass of its own.
//
// - THE ATLAS (`mark_atlas`): engine/detail's masks, kMarkVariants a kind at kMarkAtlasPx a side;
//   `mark_beam_settings` makes the beams pass with it and room for a pool's burns.
// - A BLAST'S SCORCH (`press_scorch`): render/explosion.hpp's `scorch_of` pressed as a scorch decal
//   at the blast's time, so its embers glow and fade from the moment it went off.
// - THE BURNS (`mark_burns`): every live decal of a pool its triangles' vertices in the atlas's
//   cell, its soot (how dark it makes what is under it) and its embers (kHotEmberCd at their
//   hottest) as they are at the moment drawn; a bullet hole shaped by its mask's relief, lit along
//   the world's up across it by the light each slot lies in (a game's, whites; 1 without).
// - A GLOWING MARK (`press_glow`): a hit that melts the wall where it lands (a rail's slug, a big
//   energy ball's burst), a melt decal `size_m` across that glows white-hot and cools to black over
//   `glow_s`; a held beam's line and melt are the same marks (render/beam.hpp's `beam_burns`).
// - THE MARKS' LIGHT (`mark_glows`): every mark still glowing as a light off the wall, its colour
//   the ember's by its heat and its level by its heat and size, the near ones merged into one (a
//   beam's line of pieces is one light that follows it), the brightest `most` kept, so a hot mark
//   lights a dark room for the seconds it glows.
//
// ge:summary The decals drawn as the beams pass's burns: the atlas, a blast's scorch, the burns
// ge:keywords render marks decals burns scorch bullet-hole blood atlas mark_atlas mark_burns
// ge:keywords press_scorch mark_beam_settings ember soot relief
// ge:keywords press_glow mark_glows glow-light melt
// ge:skill render detail
// ge:test tests/render_marks_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "detail/decals.hpp"
#include "render/beams.hpp"
#include "render/explosion.hpp"

namespace ge::render {

inline constexpr int kMarkVariants = 4;     // the atlas's masks a kind (render's kScorchVariants)
inline constexpr int kMarkAtlasPx = 128;    // a mask's side in it
inline constexpr float kHotEmberCd = 6000;  // a fresh scorch's glow, cd/m^2
inline constexpr float kHotGlowKeys = 6;    // a fresh burn line's or melt's glow, whites
static_assert(kMarkVariants == static_cast<int>(kScorchVariants));

// engine/detail's `bake_atlas` of kMarkVariants masks a kind, made once.
const std::vector<uint8_t>& mark_atlas();
// The beams pass's settings to draw the burns of a pool of `marks` decals with the marks' atlas.
BeamSettings mark_beam_settings(uint32_t marks);

// The scorch of `blast` pressed onto `receivers` at the blast's time; its slot, or -1 when the
// blast leaves none (in the open air) or no surface under it faces its way.
int press_scorch(detail::DecalPool& pool, const detail::Receivers& receivers, const Blast& blast);

// Every live decal of `pool` at `now_s` as burn vertices into `out` (cleared first). `light`, one
// a slot, is the light each lies in (whites); empty: 1.
void mark_burns(const detail::DecalPool& pool, double now_s,
                std::span<const std::array<float, 3>> light, std::vector<BurnVertex>& out);

// A glowing mark's size and how long it glows.
struct GlowMark {
  double size_m = 0.3;
  double glow_s = 3;
};
// A melt `mark.size_m` across pressed at `at` (world m) on the surface out along `normal` at
// `now_s`; its slot, or -1 when no surface under it faces its way.
int press_glow(detail::DecalPool& pool, const detail::Receivers& receivers, const double at[3],
               const double normal[3], const GlowMark& mark, double now_s, uint32_t variant);

// An ember's colour by its heat (1 fresh, 0 cold): yellow-white, orange, a dull red (linear, the
// beams pass's burn shader's own).
std::array<float, 3> ember_rgb(float heat);

// One light the glowing marks give: its place (a little off the wall), its colour times its level
// (1: a fresh mark of kGlowAreaM2 at full heat) and where its light ends.
struct MarkGlow {
  double at[3] = {0, 0, 0};
  float rgb[3] = {0, 0, 0};
  float reach_m = 0;
};
inline constexpr double kGlowAreaM2 = 0.1;  // a mark this big, fresh, glows at a level of 1
inline constexpr double kGlowMergeM = 0.9;  // glows nearer than this to a kept one join it
// The lights of `pool`'s glowing marks at `now_s` into `out` (at most `most`), brightest first;
// returns how many.
int mark_glows(const detail::DecalPool& pool, double now_s, MarkGlow* out, int most);

}  // namespace ge::render
