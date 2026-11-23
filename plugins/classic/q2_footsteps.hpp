// QUAKE II'S FOOTSTEPS BY SURFACE (`cl_footsteps` 1): each wall texture may name a MATERIAL in a
// text file of its own, `textures/<texture>.mat` (its first 15 characters; letters, digits, `_`
// and `-` only, else a warning and no material), and a step on it plays that material's set
// instead of the game's own four. The sets of one level (`q2_step_sets`):
//
// - set 0, the default: `player/step1.wav`, `player/step2.wav` ... (a texture with no file, an
//   empty one or the material `default`);
// - set 1, the ladder: `player/steps/ladder1.wav` ... (the material `ladder`);
// - one set a material: `player/steps/<material>1.wav` ... (materials compared without case).
//
// A set is numbered from 1 up to the first file missing, 15 at most; a set with no file plays the
// default's, and an empty default plays nothing. THE SURFACE under a stepping entity
// (`q2_step_surface`): a flat box its horizontal size swept down from 1 unit above its origin,
// to 9 units under its box's bottom (75 under the start for an entity with no box); solids only,
// then again to 1 unit above that hit with liquids too, so shallow water's surface wins. A level
// with no material beyond default and ladder skips the search. THE VARIANT (`q2_step_pick`): one
// of the set at random, the next in turn when it is the step heard last (anyone's), so a sound is
// never heard twice running while its set holds two. `cl_footsteps` 2 always plays the default.
//
// ge:summary Quake II's footsteps by surface: .mat materials, step sets, the surface underfoot
// ge:keywords cl_footsteps footsteps step-sets material mat-file surface-sound ladder-steps
// ge:keywords q2_step_sets q2_step_surface q2_step_pick player-steps
// ge:skill classic audio
// ge:test tests/classic_q2_footsteps_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "brush/trace.hpp"

namespace ge::classic {

inline constexpr int kQ2StepVariants = 15;  // the most files a step set holds
inline constexpr int kQ2StepDefault = 0, kQ2StepLadder = 1;

struct Q2StepSets {
  std::vector<std::vector<std::string>> sets;  // by set: the sounds' names, as the game names them
  std::vector<std::string> materials;          // by set: the material ("" default, "ladder")
  std::vector<int> by_texture;                 // the map's texture index to its set
  bool any_material = false;                   // a material beyond default and ladder
};

// The step sets of `map`'s textures: `read_text` gives a file's text (nullopt: none),
// `has_sound` says whether a sound by the game's name is there; a bad material is said in `said`
// ("Bad material ..."), once a texture name.
Q2StepSets q2_step_sets(
    const brush::Map& map,
    const std::function<std::optional<std::string>(const std::string&)>& read_text,
    const std::function<bool(const std::string&)>& has_sound,
    std::vector<std::string>* said = nullptr);

// The texture under an entity at `origin` (map units) with box `mins`..`maxs` (null: none); -1
// when nothing is there.
int32_t q2_step_surface(brush::BrushTracer& tracer, const float origin[3], const float* mins,
                        const float* maxs);

// The set a step on texture `texture` plays (the default for -1 or an unknown one).
int q2_step_set(const Q2StepSets& s, int32_t texture);

// One sound of set `set` (falling to the default when it is empty): `roll` picks it, `last` (the
// step heard last, "" none) moves it on by one. Null when the default is empty too.
const std::string* q2_step_pick(const Q2StepSets& s, int set, uint32_t roll, std::string_view last);

// Quake II's packed box (DemoEntity::solid: half width, depth and height over 32 in eighths of 8;
// 0 not solid, 31 a brush model) as mins and maxs; false when it is no box.
bool q2_unpack_solid(int solid, float mins[3], float maxs[3]);

}  // namespace ge::classic
