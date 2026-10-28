// THE TYPE A HUD WRITES IN: one of two kinds of font behind one interface, so a game picks its
// HUD's look by data and its layout code never asks which. A FACE is either a TrueType font of the
// 2D layer (ui/font.hpp: Barlow Condensed built in, or a game's own) or one of the engine's bitmap
// charsets (charset/charset.hpp: ten monospaced ASCII looks), drawn into the same draw list.
//
// A third kind, a GLYPH SHEET (a game's own 16 x 16 picture of square cells), is drawn the same
// way: a cell a character, sampled nearest, its line the cell's height sitting on the baseline.
//
// SIZE means the same for both: the line's pixel height (a TrueType font's ascent plus descent; a
// charset's whole cell, whose baseline sits 0.72 of the way down). A run is placed by its
// BASELINE, as the 2D layer places text, so a number in one face and its unit in another sit on
// one line.
//
// A charset run is drawn as one image box a character over the charset's atlas (white, the
// coverage its alpha, tinted by the run's colour): the CPU raster and the GPU pass draw it like any
// other image. The atlases are built on first use, the smallest committed size that is only ever
// shrunk for the run's height (`charset::atlas_size_for`), and kept: this file's one piece of
// module state, read and filled from the frame's thread only, as the 2D layer's glyph cache is.
//
// The charset's UI symbols (a star, a heart, a diamond, the arrows, a bullet) are drawn the same
// way by `draw_symbol`: a wanted level's stars, a shooter's hearts, a pip of ammunition.
//
// ge:summary The HUD's type: a TrueType font or a bitmap charset behind one size and baseline
// ge:keywords hud face font charset bitmap-font truetype text measure draw baseline ascent
// ge:keywords symbol star heart icon atlas-image glyph tint draw_text text_width draw_symbol
// ge:keywords glyph-sheet sheet-font conchars digits_at
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#pragma once

#include <string_view>

#include "charset/charset.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"

namespace ge::hud {

struct Face {
  const ui::Font* font = nullptr;  // a TrueType font, or nullptr for the charset below
  charset::FontId charset = charset::FontId::rounded;
  // A game's own glyph SHEET instead of both: a picture of 16 x 16 square cells, the byte's
  // cell in reading order, drawn nearest at a whole scale. `digits_at` is the cell of '0' (a
  // sheet may hold a second set of figures in another colour: its digits are drawn from there).
  const ui::Image* sheet = nullptr;
  uint8_t digits_at = '0';

  static Face truetype(const ui::Font& f) { return Face{&f, charset::FontId::rounded}; }
  static Face bitmap(charset::FontId id) { return Face{nullptr, id}; }
  static Face glyph_sheet(const ui::Image& img, uint8_t digits_at = '0') {
    return Face{nullptr, charset::FontId::rounded, &img, digits_at};
  }
};

// Whether `u` px per picture pixel is (all but) a whole number of at least 1, so a picture drawn
// at it samples nearest and stays crisp; a fractional scale blends, so it does not shimmer.
bool whole_unit(float u);

// Px above and below the baseline a run of `size` takes.
float ascent(const Face& face, float size);
float descent(const Face& face, float size);

// The run's advance width, px (`tracking` px after every character).
float text_width(const Face& face, float size, std::string_view text, float tracking = 0);

// `text` with its left edge, centre or right edge at x (by `align`) and its baseline at y. Returns
// its advance width. A charset run draws ASCII; every other byte draws the missing glyph.
float draw_text(ui::DrawList& dl, const Face& face, float size, float x, float baseline,
                std::string_view text, ui::Color color, ui::Align align = ui::Align::left,
                float tracking = 0);

// The run over its shadow: an outline in `shadow` (four copies `drop` * 0.6 px off each way) and
// a copy `drop` px lower, then the run in `color` over them. Words over a bright snowfield and a
// dark wood both read with no plate behind them.
float draw_shadowed(ui::DrawList& dl, const Face& face, float size, float x, float baseline,
                    std::string_view text, ui::Color color, ui::Color shadow, float drop,
                    ui::Align align = ui::Align::left, float tracking = 0);

// One of the charset's UI symbols (`charset::Sym`) or any code, drawn to fill the box `cell` (a
// charset cell is twice as tall as it is wide: give the box that shape to keep the symbol's).
void draw_symbol(ui::DrawList& dl, charset::FontId font, uint8_t code, ui::Rect cell,
                 ui::Color color);

// The atlas of `font` at `size` as a 2D layer image (white, its coverage the alpha), built on
// first use and kept.
const ui::Image& atlas_image(charset::FontId font, int size);

}  // namespace ge::hud
