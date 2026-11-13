// THE CHARSETS AS FONTS THE 2D LAYER WRITES WITH: each of the engine's ten bitmap fonts
// (engine/charset, docs/charsets.md) as a grid `Font` (font.hpp's `load_grid`) over its atlases
// at 256, 512 and 1024 px, built the first time it is asked for and kept. A text run in a
// charset is drawn by the CPU raster and the GPU's 2D pass as any other: the font's masks are its
// cells scaled to the run's size. The console's `con_font` names one (ui/console.hpp).
//
// A GAME'S OWN GLYPH SHEET is a grid font too (`sheet_font`): a picture of 16 x 16 square cells,
// the byte's cell in reading order, each cell's coverage its pixels' brightness (the sheet's
// see-through pixels none), so the sheet's own shading shows under the text's colour and a
// classic game's console and messages are written in that game's letters, drawn at whole
// multiples of its cell.
//
// ge:summary The engine's ten charsets as grid fonts the 2D layer draws text runs with
// ge:keywords ui charset font bitmap-font grid-font monospace terminal mono atlas con_font
// ge:keywords glyph-sheet sheet_font
// ge:skill ui
// ge:test tests/ui_charset_font_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "ui/font.hpp"
#include "ui/image.hpp"

namespace ge::ui {

// The charset named `name` ("terminal", "mono", ... charset::fonts()) as a grid font, built on
// the first call and kept; nullptr when no charset has that name. One thread (the frame's).
const Font* charset_font(std::string_view name);

// `sheet` (RGBA, square, 16 x 16 cells) as a grid font into `out`. False, with the reason in
// `why`, when the sheet is empty or not square in whole cells.
bool sheet_font(const Image& sheet, Font& out, std::string* why = nullptr);

}  // namespace ge::ui
