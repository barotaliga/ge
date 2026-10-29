// THE DROP-DOWN CONSOLE DRAWN: the console's model (console/view.hpp) turned into this frame's
// draw list, so the CPU raster (the `console` lab) and the GPU pass (M22b) draw it alike. Down,
// it is a panel from the top of the screen at the view's cover, on the con_bg* background (a
// gradient, an optional image over it), its lines coloured by kind with the newest just over the
// input line; the input line holds `] `, the typed text, the hint (the rest of the name Tab would
// take, dim) and the cursor; the suggestions hang under the panel's edge with the selected row
// lit. Up, only the notify lines show, each on a dark band at the top left.
//
// Sizes are at a 720 px tall reference and scale with the target's height, so a console reads
// the same on every screen. The look is a `ConsoleStyle` a game sets; nothing here is Fall
// Line's. The text is in con_font's charset (ui/charset_font.hpp; mono by default), or the
// style's font when it names one, or the built-in UI font when con_font names no charset.
//
// ge:summary The drop-down console as a draw list: panel, background, lines, input, hint, list
// ge:keywords ui console draw drop-down panel notify suggestion hint cursor scrollback
// ge:keywords con_bgtop con_bgbottom con_bgalpha con_bgimage
// ge:skill ui console
// ge:test tests/ui_console_test.cpp
#pragma once

#include <array>

#include "console/view.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"
#include "ui/paint.hpp"

namespace ge::ui {

class Font;

struct ConsoleStyle {
  const Font* font = nullptr;  // null: con_font's charset (the UI font when it names none)
  // At the 720 px reference.
  float text_px = 18, row_px = 22, margin_px = 12, edge_px = 2;
  // One colour a line kind, in console::LineKind's order.
  std::array<Color, 8> kinds{hex(0xc8c8c8), hex(0xffffff), hex(0x78e678), hex(0x6ed2ff),
                             hex(0xf08cf0), hex(0xc8e6a0), hex(0xfadc5a), hex(0xffa046)};
  Color input = hex(0xffffff);
  Color hint = hex(0x78829a);
  Color cursor = hex(0xffffff);
  Color edge = hex(0xfadc5a);
  Color list_fill = hex(0x10141c, 0.92f);
  Color list_lit = hex(0x3c5078);
  Color list_name = hex(0xdce1eb);
  Color list_detail = hex(0x828ca0);
  Color notify_band = hex(0x10141c, 0.55f);
  float cursor_blink_s = 0.5f;  // half a period; 0: always on
};

// The console as it stands, into `list` (begun at the target's size). `image` is con_bgimage's
// picture, loaded by the caller (null: the gradient alone). `time_s` blinks the cursor.
void draw_console(DrawList& list, const console::ConsoleView& view, const ConsoleStyle& style = {},
                  const Image* image = nullptr, double time_s = 0);

}  // namespace ge::ui
