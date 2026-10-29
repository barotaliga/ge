// THE CHARSETS AS FONTS THE 2D LAYER WRITES WITH: each of the engine's ten bitmap fonts
// (engine/charset, docs/charsets.md) as a grid `Font` (font.hpp's `load_grid`) over its atlases
// at 256, 512 and 1024 px, built the first time it is asked for and kept. A text run in a
// charset is drawn by the CPU raster and the GPU's 2D pass as any other: the font's masks are its
// cells scaled to the run's size. The console's `con_font` names one (ui/console.hpp).
//
// ge:summary The engine's ten charsets as grid fonts the 2D layer draws text runs with
// ge:keywords ui charset font bitmap-font grid-font monospace terminal mono atlas con_font
// ge:skill ui
// ge:test tests/ui_charset_font_test.cpp
#pragma once

#include <string_view>

#include "ui/font.hpp"

namespace ge::ui {

// The charset named `name` ("terminal", "mono", ... charset::fonts()) as a grid font, built on
// the first call and kept; nullptr when no charset has that name. One thread (the frame's).
const Font* charset_font(std::string_view name);

}  // namespace ge::ui
