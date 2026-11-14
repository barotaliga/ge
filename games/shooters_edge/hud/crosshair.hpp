// THE CROSSHAIR as the player sets it: which mark (`crosshair`: 0 none, 1 the set's own, 2..7 the
// built marks below), its colour (`crosshaircolor`: empty its own, one number a colour of the
// classic palette, or "R G B" 0..255), its size (`crosshairsize`, times the set's) and how solid it
// is
// (`crosshairalpha`). The built marks are painted white in code, so the colour tints them as
// given; the set's own keeps its picture's colours under the tint.
//
//   2 a dot          3 a cross       4 a cross with a gap at the middle
//   5 a ring         6 a ring and a dot          7 a cross turned on its side (an X)
//
// ge:summary Shooter's Edge's crosshair settings: the mark, its colour, size and opacity
// ge:keywords crosshair crosshaircolor crosshairsize crosshairalpha crosshair-style
// ge:keywords crosshair-marks palette-index fill_crosshair
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "hud/layout.hpp"
#include "shooters_edge/hud/art.hpp"
#include "ui/image.hpp"

namespace shooters_edge::hud {

inline constexpr int32_t kCrosshairMarks = 7;  // the highest `crosshair`

struct CrosshairSettings {
  int32_t mark = 1;            // crosshair
  std::string colour;          // crosshaircolor ("": the mark's own colours)
  double size = 1, alpha = 1;  // crosshairsize, crosshairalpha
};

// Registers crosshair, crosshaircolor, crosshairsize and crosshairalpha (`s` must outlive `con`).
void add_crosshair_cvars(ge::console::Console& con, CrosshairSettings& s);

// The colour `text` names: one number a palette index (the set's palette, else a grey of that
// level), or "R G B" 0..255; white (no tint) for nothing or anything else.
ge::ui::Color crosshair_colour(std::string_view text, const HudArt& art);

// Built mark `mark` (2..kCrosshairMarks), painted white at kMarkDensity pixels per art pixel; an
// empty picture for any other.
inline constexpr float kMarkDensity = 4;
const ge::ui::Image& crosshair_mark(int32_t mark);

// The crosshair element's content this frame from the settings and the set (`dead`: none).
void fill_crosshair(ge::hud::Content& c, const CrosshairSettings& s, const HudArt& art, bool dead);

}  // namespace shooters_edge::hud
