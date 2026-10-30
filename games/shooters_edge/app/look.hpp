// SHOOTER'S EDGE'S TWO LOOKS (docs/shooters-edge.md § The picture): one switch over the world's
// renderer (brush/app/bsp_draw.hpp) and the post pass (render/post.hpp).
//
// - CLASSIC: the maps as they were made to look: each texel a crisp square, the lightmaps' light
//   and its styles, the liquids warped, the map's own sky; no bloom; and the picture's colours
//   pulled to the nearest of the game's 256 by the post pass's grade (`palette_grade`).
// - MODERN: the same faces under the engine's world: filtered textures, real-time lights with
//   shadows, our sky, water, lava and slime, and bloom.
//
// ge:summary Shooter's Edge's classic and modern looks: the world's options, the post's, the
// palette grade
// ge:keywords look classic modern r_look palette grade quantize crisp bloom shooter picture
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "render/post.hpp"

namespace shooters_edge::look {

enum class Look : uint8_t { classic, modern };
// "classic" or "modern"; false for any other word.
bool parse_look(std::string_view word, Look* out);
const char* look_name(Look look);

// The world renderer's options in `look`, over `base` (the game's other settings).
ge::brush::app::Options world_options(Look look, ge::brush::app::Options base = {});
// The post pass's settings in `look`.
ge::render::PostSettings post_settings(Look look);
// The classic grade: every entry the nearest (in sRGB) of `palette`'s colours (768 bytes, RGB), in
// render/post.hpp's `identity_grade` layout. An empty palette gives the identity.
std::vector<float> palette_grade(std::span<const uint8_t> palette);

}  // namespace shooters_edge::look
