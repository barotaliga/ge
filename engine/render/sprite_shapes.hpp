// SPRITE SHAPES MADE, NOT LOADED (docs/rendering.md § Sprites): the frames a game draws in place
// of a pack's flat effect sprites (classic-enhance's replacement table names `flare` and
// `bubbles`), made once at load as pictures for the sprites pass.
//
// - A FLARE (`flare_frame`): a light's glow seen through the air and the eye's lens, a bright
//   round core falling off as a Gaussian, a wide faint halo round it, white; drawn `add`ed with the
//   light's colour on the draw, so it lights nothing and only glows.
// - A BUBBLE (`bubble_frame`): a ball of air in water seen against what is behind it, nearly clear
//   in its middle, its rim bright where the light glances off it (more so the nearer the edge, a
//   Fresnel rim), a hot highlight up and to the left where the light above meets it, and a faint
//   darker ring just inside the rim where the light is bent away; drawn `over`.
//
// Both are pure functions of their size: the same texels every time.
//
// ge:summary Sprite frames made at load: a flare's glow and an underwater bubble
// ge:keywords render sprite flare glow halo bubble underwater fresnel rim highlight procedural
// ge:keywords flare_frame bubble_frame replacement
// ge:skill render
// ge:test tests/render_sprite_shapes_test.cpp
#pragma once

#include <cstdint>

#include "render/sprites.hpp"

namespace ge::render {

// A flare `px` texels a side (at least 8), its origin the middle.
SpriteFrame flare_frame(uint32_t px = 64);
// A bubble `px` texels a side (at least 8), its origin the middle.
SpriteFrame bubble_frame(uint32_t px = 32);

}  // namespace ge::render
