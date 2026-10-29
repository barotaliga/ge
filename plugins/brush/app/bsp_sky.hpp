// A CLASSIC MAP'S SKY AS OURS (the v2 BSP renderer, bsp_draw.hpp): our sky picked to suit the
// map's own sky texture. The texture's colours (draw_data.hpp's `SkyColours`) say what kind of sky
// it is: its hue picks a sky type (render/sky_style.hpp: a red smoke is a hellish air, a violet
// dusk a nebula's, a green haze a toxic one), its brightness the hour and how bright the sky is
// against the rooms (a dark sky a dusk, veiled dim over torch-lit halls), its cloud layer the
// heaps' cover, and the type's air is tinted toward the texture's colour so the sky through the sky
// faces reads as the map's.
//
// ge:summary Our sky picked by a classic map's sky texture: its type, light, tint and cloud
// ge:keywords bsp brush sky sky-type match pick_sky sky-texture hue dusk cover v2
// ge:skill brush render
// ge:test tests/brush_bsp_sky_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "brush/app/draw_data.hpp"
#include "render/sky_style.hpp"

namespace ge::brush::app {

struct SkyPick {
  std::string type = "earth";  // the sky type it starts from (render/sky_style.hpp)
  render::SkyStyle style;      // that type, its air tinted toward the texture's colour
  double sun_elevation = 30;   // degrees
  double sky_light = 1;        // the sky against the rooms (Options::sky_light, r_skylight)
  double tint[3] = {1, 1, 1};  // the sky's tint toward the texture's hue (Options::sky_tint)
  double cover = 0.3;          // the heaps' share of the sky
  double cell_m = 400;         // a heap's span, m (the classic's small clouds)
  std::string why;             // one line: what in the texture chose it
};

// Our sky for a map whose sky texture has `colours` (no texture: the Earth's fair day).
SkyPick pick_sky(const SkyColours& colours);

// `pick` with the type `name` in place of its own (its tint and hour kept); false for an unknown
// word.
bool with_type(SkyPick& pick, std::string_view name);

}  // namespace ge::brush::app
