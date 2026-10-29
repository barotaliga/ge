// THE LEVEL OF DETAIL (docs/snow.md § Level of detail). A layer keeps fine tiles only where
// someone is: the game names its FOCUS actors (each player, each machine with a rider) every step
// with `set_focus`, never the camera, so a server, its clients and a replay keep the same tiles.
//
// - An imprint farther than `LayerSpec::near` from every focus, over ground no fine tile holds,
//   lands in the COARSE LAYER (`press` does it): the coarse cells it covers are worn by the share
//   of each it covers, their mass kept and their surface lowered.
// - `fold_stale` folds a fine tile no focus has come near for `LayerSpec::stale` seconds into the
//   coarse cells under it: each coarse cell takes the SUM of its samples' mass units and depths,
//   so the mass is kept to the bit.
// - A fine tile allocated over coarse cells is REFINED from them: each coarse cell's units spread
//   evenly over its samples (the remainder a unit each to the first, in sample order), so a skier
//   coming back to a run he tracked an hour ago finds it worn, not untouched.
// - Past `LayerSpec::max_tiles` the fine tile least recently near a focus is folded to make room
//   (ties by slot, so the order is the same on every machine). A tile written in the operation in
//   progress is never folded by it; when no other is left, the sample is refused as before.
//
// Nothing here reads a clock: the moment is the game's, passed in.
//
// ge:summary Snow level of detail: focus actors, the coarse layer, folding and refining tiles
// ge:keywords snow lod level-of-detail focus coarse fold refine budget tiles stale near memory
// ge:keywords set_focus fold_stale fold_tile press_coarse bytes_of
// ge:skill snow
// ge:test tests/snow_lod_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "snow/layer.hpp"
#include "snow/press.hpp"

namespace ge::snow {

// The focus actors at `t`, as x, z pairs: every live fine tile within `near` of one is stamped
// near at `t`. An empty span means no focus: every imprint keeps fine detail.
void set_focus(Layer& layer, std::span<const double> xz, double t);
// Whether (x, z) lies farther than `near` from every focus (false with no focus).
bool far_from_focus(const Layer& layer, double x, double z);

// Folds every live fine tile not near a focus since `t - stale`, in slot order; returns how many.
int fold_stale(Layer& layer, double t);
// Folds the tile in `slot` into the coarse layer and frees the slot.
void fold_tile(Layer& layer, int slot);
// Fills a newly allocated tile's samples from the coarse cells over it where they are touched
// (the cover elsewhere); `cell_for` calls it.
void refine_tile(Layer& layer, Tile& tile);
// The slot of the live tile least recently near a focus and written before `layer.clock`, or -1.
int stalest_tile(const Layer& layer);

// An imprint far from every focus, pressed into the coarse layer: each coarse cell it touches
// sinks by the share of it the outline covers. `press` calls it.
Pressed press_coarse(Layer& layer, const Imprint& imp, double t);

// The memory the layer's tiles, coarse blocks and records hold, bytes.
size_t bytes_of(const Layer& layer);
// Live fine tiles.
int live_tiles(const Layer& layer);

}  // namespace ge::snow
