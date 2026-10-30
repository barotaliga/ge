// THE HUD'S PICTURE WIDGETS: what a HUD drawn from a game's ART draws, beside the kit's printed
// widgets (hud.hpp). A shooter's status bar is pictures: a panel behind everything, an icon for
// the armour and the ammunition, a portrait that bleeds as health falls, numbers drawn from a
// sheet of digit pictures, a row of weapon icons (the one in hand lit, the newly picked up one
// flashing) with the rounds left over them, keys and powerups in fixed slots. A modern HUD draws
// a ring gauge round its crosshair. Each is measured, then drawn into the box its measure was
// placed at, as the kit's are.
//
// THE UNIT. Pictures are laid out in ART PIXELS: `u` is screen px per art pixel, which the layout
// works from the frame's scale and the element's unit (layout.hpp), so a 24 px icon at u = 3 is a
// 72 px box. A picture may be painted finer than its art pixels: its DENSITY is picture pixels
// per art pixel (a 96 px painting of a 24 px icon has density 4), so the same layout takes a
// low-resolution set and a sharp one. A picture is sampled NEAREST (`ui::Box::nearest`) when
// u / density is a whole number, so pixel art stays crisp; otherwise it blends, so it does not
// shimmer.
//
// A DIGIT SHEET is eleven pictures, 0..9 and the minus, and an optional ALTERNATE eleven (the
// classic way to say "low": the same figures in red). A number is drawn right-aligned in a fixed
// count of cells, as a status bar's 3-digit fields are, and a character the sheet lacks leaves
// its cell empty.
//
// A STRIP is a row (or a column) of cells, each a picture with its state: shown or an empty slot,
// faded, tinted, framed (the one in hand), with an optional plate of two colours behind it (a
// player's colours under his frag count) and an optional count over it in the strip's face.
// A cell's width is the strip's, or its picture's own when the strip's is 0 (one wide icon in a
// row of narrow ones).
//
// A RING is a meter bent round a centre: a track and a fill along an arc, 0 at its `from` end;
// a level past 1 fills a second lap in the over colour, as a health past its maximum does.
//
// ge:summary The HUD's picture widgets: pictures, digit sheets, icon strips and ring gauges
// ge:keywords hud picture image icon digits digit-sheet number status-bar strip row cell slot
// ge:keywords inventory weapons ammo keys powerups frags plate ring arc gauge meter nearest
// ge:keywords pixel-art unit alternate low-figures
// ge:skill ui
// ge:test tests/hud_pictures_test.cpp
#pragma once

#include <array>
#include <string>
#include <vector>

#include "hud/face.hpp"
#include "hud/hud.hpp"
#include "ui/draw.hpp"

namespace ge::hud {

// One picture, tinted.
struct Picture {
  const ui::Image* image = nullptr;
  ui::Color tint{1, 1, 1, 1};
  float density = 1;
};
Size measure(const Picture& p, float u);
void draw(ui::DrawList& dl, const Picture& p, ui::Rect at, float u);

// 0..9 then the minus; `alt` the same in the alternate colour (empty pointers fall back to the
// plain sheet's).
struct DigitSheet {
  std::array<const ui::Image*, 11> glyph{};
  std::array<const ui::Image*, 11> alt{};
  float density = 1;
  // The size of a cell, art px: glyph 0's.
  Size cell() const;
};

struct Digits {
  const DigitSheet* sheet = nullptr;
  std::string value;  // digits and '-'; anything else is an empty cell
  int cells = 3;      // the field's width in cells; 0: as many as the value has
  bool alt = false;
  ui::Color tint{1, 1, 1, 1};
};
Size measure(const Digits& d, float u);
void draw(ui::DrawList& dl, const Digits& d, ui::Rect at, float u);

struct Cell {
  const ui::Image* image = nullptr;
  bool show = true;  // false: an empty slot that keeps its place
  float alpha = 1;
  ui::Color tint{1, 1, 1, 1};
  bool framed = false;              // a frame round the cell in `frame` (the one in hand)
  ui::Color plate{0, 0, 0, 0};      // behind the cell's top part, when its alpha is above 0
  ui::Color plate_low{0, 0, 0, 0};  // and its lower part (a player's two colours)
  std::string text;                 // a count over the cell
  ui::Color text_color{1, 1, 1, 1};
};

// A strip's shape, in art px.
struct StripShape {
  float cell_w = 24, cell_h = 16;  // 0: the cell's picture's own size
  float gap = 0;
  bool vertical = false;
  float text_size = 8;                // the count's type
  Anchor text_at = Anchor::top_left;  // where in the cell the count hangs
  float text_dx = 0, text_dy = 0;     // and moved by
  float plate_h = 0;                  // the plate's height (0: the cell's)
  float plate_split = 0.57f;          // the share of the plate in its top colour
  ui::Color frame{1, 1, 1, 1};
  float frame_w = 1;
};

struct Strip {
  const std::vector<Cell>* cells = nullptr;
  float density = 1;  // the cells' pictures'
  Face face = Face::bitmap(charset::FontId::rounded);
  ui::Color shadow{0, 0, 0, 0};  // under the counts, when its alpha is above 0
};
Size measure(const StripShape& s, const Strip& strip, float u);
void draw(ui::DrawList& dl, const StripShape& s, const Strip& strip, ui::Rect at, float u);

// A ring gauge's shape, reference px and degrees (0 at twelve o'clock, clockwise).
struct RingShape {
  float radius = 60, width = 6;
  float from_deg = 210, to_deg = 330;  // clockwise from `from` to `to`
  bool reverse = false;                // fill from the `to` end back
  bool round = true;
};
struct Ring {
  float level = 0;  // 0..2: past 1 a second lap in `over`
  ui::Color fill{1, 1, 1, 1};
  ui::Color over{1, 1, 1, 1};
  ui::Color track{1, 1, 1, 0.25f};
};
// The ring's box is the square round its whole circle, so a ring placed at the centre is centred
// on the screen's middle whatever part of it is drawn.
Size measure(const Frame& f, const RingShape& s);
void draw(ui::DrawList& dl, const Frame& f, const RingShape& s, const Ring& r, ui::Rect at);

}  // namespace ge::hud
