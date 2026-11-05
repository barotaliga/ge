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
//
// ge:summary The decals drawn as the beams pass's burns: the atlas, a blast's scorch, the burns
// ge:keywords render marks decals burns scorch bullet-hole blood atlas mark_atlas mark_burns
// ge:keywords press_scorch mark_beam_settings ember soot relief
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

}  // namespace ge::render
