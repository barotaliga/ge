// SHOOTER'S EDGE'S TWO LOOKS (docs/shooters-edge.md § The picture): one switch over the world's
// renderer (brush/app/bsp_draw.hpp) and the post pass (render/post.hpp).
//
// - CLASSIC: the maps as they were made to look: each texel a crisp square, the lightmaps' light
//   and its styles, the liquids warped, the map's own sky; no bloom; and the picture's colours
//   pulled to the nearest of the game's 256 by the post pass's grade (`palette_grade`).
// - MODERN: the same faces under the engine's world: filtered textures, real-time lights with
//   shadows, our sky, water, lava and slime, and bloom.
//
// The player tunes the picture with the r_ cvars `add_cvars` registers over a `Picture`: r_look picks
// the look, and r_fx_lights, r_fx_glow and r_bloom set how much light the effects throw, how bright
// a spark glows and how much bloom the modern look mixes in (the classic keeps its own: full
// lights, full glow, no bloom).
//
// ge:summary Shooter's Edge's classic and modern looks: the world's options, the post's, the
// palette grade
// ge:keywords look classic modern r_look r_fx_lights r_fx_glow r_bloom cvars palette grade quantize
// crisp bloom shooter picture
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <string>

#include "brush/app/bsp_draw.hpp"
#include "console/console.hpp"
#include "render/post.hpp"

namespace shooters_edge::look {

enum class Look : uint8_t { classic, modern };
// "classic" or "modern"; false for any other word.
bool parse_look(std::string_view word, Look* out);
const char* look_name(Look look);

// The picture as the player set it (the r_ cvars); the defaults are the modern look's tuning.
struct Picture {
  std::string look = "classic";  // r_look
  double fx_lights = 0.2;        // r_fx_lights: the dynamic lights' strength, the classic's 1
  double fx_glow = 0.15;         // r_fx_glow: the glowing particles' light, the classic's 1
  double bloom = 0.3;            // r_bloom: the bloom's share, 0 none
};
// Registers r_look, r_fx_lights, r_fx_glow and r_bloom over `picture` (it must outlive `con`).
void add_cvars(ge::console::Console& con, Picture& picture);
// r_look's look; classic for a word it does not know.
Look look_of(const Picture& picture);

// The world renderer's options in `look`, over `base` (the game's other settings).
ge::brush::app::Options world_options(Look look, ge::brush::app::Options base = {},
                                      const Picture& picture = {});
// The glowing particles' share of their light in `look` (render/particles.hpp's
// ParticleSettings::glow): the modern's exposure and bloom would blow the sparks out at full, so
// it takes r_fx_glow.
double effects_glow(Look look, const Picture& picture = {});
// The post pass's settings in `look`.
ge::render::PostSettings post_settings(Look look, const Picture& picture = {});
// The classic grade: every entry the nearest (in sRGB) of `palette`'s colours (768 bytes, RGB), in
// render/post.hpp's `identity_grade` layout. An empty palette gives the identity.
std::vector<float> palette_grade(std::span<const uint8_t> palette);

}  // namespace shooters_edge::look
