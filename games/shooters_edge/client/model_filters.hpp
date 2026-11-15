// THE THINGS A PLAYER HIDES (the shooter look's switches, read here because only the game knows
// which models they are): `cl_gibfilter` hides the pieces a body bursts into, each model whose file
// flags it to bleed as it flies (app/effects.hpp's `kTrailGib`: the gibs and the heads); a zombie's
// thrown flesh is its weapon and stays. `cl_deadbodyfilter` hides a player's body once it lies
// still: a player's figure (the game's own or a folder's) on the last frame of a run of death
// frames (app/figure_frames.hpp's `resting_frames`, kept on each figure's `ModelKind::resting`); a
// falling body still shows. `r_drawflame 0` hides the torches' and braziers' flames (a model loaded
// with a bare form, or drawn as the engine's fire), their burning in either look.
// world_models.hpp's `filtered_models` applies it to a frame's models.
//
// ge:summary The models a player hides: gibs, still bodies and flames (cl_gibfilter and kin)
// ge:keywords cl_gibfilter cl_deadbodyfilter gibs dead-body corpse hidden_by hidden-models
// ge:keywords r_drawflame flames torches
// ge:skill render shooter
// ge:test tests/shooters_edge_model_filters_test.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace shooters_edge::client {

struct ModelFilters {
  bool gibs = false;          // cl_gibfilter
  bool still_bodies = false;  // cl_deadbodyfilter
  bool flames = false;        // r_drawflame 0
  bool any() const { return gibs || still_bodies || flames; }
};

// Whether a model with the file's effect `flags` and its figure's `resting` frames (empty for
// anything but a player's figure) is hidden on `frame`; `flame`: the model is a flame.
bool hidden_by(const ModelFilters& f, uint32_t flags, const std::vector<bool>& resting, int frame,
               bool flame = false);

}  // namespace shooters_edge::client
