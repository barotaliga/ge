// A MENU LAID OUT AND DRAWN: the model (menu.hpp) as a column of slabs over a backdrop
// (background.hpp), the way most games' front doors look: the game's name large at the top of
// the column, the items under it, the focused one lit in the accent colour with a highlight that
// glides to it, its hint under the column, the controls' help and the build's stamp in the
// bottom corners.
//
// LAYOUT is responsive, decided from the screen's shape, never from a device list:
//   - landscape: the column stands to one side (`MenuStyle::side`), centred down the screen,
//     at most `column_width` (and 46% of the screen) wide, leaving the rest to the backdrop's
//     subject;
//   - portrait: the column runs the screen's width inside its margins, anchored low, so the top
//     of the screen shows the backdrop (a phone held upright).
// Sizes are given at a 720 px tall reference and scaled to the screen; a page that would not fit
// is scaled down until it does. Everything stays inside `safe` (a notch, rounded corners).
//
// `layout_problems` names what a reader of a screenshot would flag: an item's words wider than
// its slab, a slab off the screen or out of the safe area, two slabs overlapping. The `menu` lab
// fails on any of them at every reference viewport.
//
// ge:summary A menu laid out (landscape column, portrait sheet) and drawn: slabs, focus, hints
// ge:keywords ui menu view layout responsive landscape portrait column slab focus highlight
// ge:keywords hint footer help logo safe-area scale style theme accent animation glide
// ge:skill ui
// ge:test tests/ui_menu_test.cpp
#pragma once

#include <string>
#include <vector>

#include "ui/background.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/menu.hpp"

namespace ge::ui {

struct MenuStyle {
  const Font* title_font = nullptr;  // none: ui_font(semibold)
  const Font* item_font = nullptr;   // none: ui_font(semibold)
  const Font* small_font = nullptr;  // none: ui_font(medium)

  std::string logo;    // the root page's title (the game's name), drawn large
  std::string help;    // bottom left: the controls ("ENTER SELECT   ESC BACK")
  std::string footer;  // bottom right: the build's stamp
  std::string on = "ON", off = "OFF";

  Side side = Side::left;  // landscape: where the column stands (left or right)

  // At the 720 px reference.
  float logo_size = 72, title_size = 40, item_size = 30, prominent_size = 38, small_size = 19;
  float item_height = 52, prominent_height = 72, gap = 8, radius = 6;
  float column_width = 440, margin = 64, pad = 20;
  float tracking = 1.5f;  // letter spacing of the slabs' words, px at the reference

  Color text = hex(0xffffff);
  Color text_dim = hex(0xffffff, 0.68f);
  Color text_disabled = hex(0xffffff, 0.32f);
  Color text_focus = hex(0xffffff);
  Color slab = hex(0x0b1622, 0.55f);
  Color slab_border = hex(0xffffff, 0.10f);
  Color accent = hex(0xd8312b);  // the focused slab
  Color accent_hi = hex(0xf0533f);
  Color track = hex(0xffffff, 0.22f);  // a slider's track
  Color shadow = hex(0x000000, 0.45f);
};

struct MenuLayout {
  int width = 0, height = 0;
  bool portrait = false;
  float scale = 1;
  Rect safe;
  Rect column;              // the title and the items
  Rect title;               // the logo or the page's title
  std::vector<Rect> items;  // index for index with the page's items
  Rect hint;
  Rect help, footer;
};

// `safe` empty: the whole screen.
MenuLayout layout_menu(const Menu& menu, int width, int height, const MenuStyle& style,
                       Rect safe = {});

// What is wrong with a layout, each a sentence naming the item and what to change; empty when
// it reads.
std::vector<std::string> layout_problems(const Menu& menu, const MenuLayout& layout,
                                         const MenuStyle& style);

// The view's animation between frames: the highlight gliding to the focus, a page fading and
// sliding in when it opens. Stepped by the caller's frame time, so a recorded run draws the same.
class MenuView {
 public:
  void frame(const Menu& menu, const MenuLayout& layout, double dt_s);
  // Settles every animation where it is going (a still with nothing in motion).
  void settle(const Menu& menu, const MenuLayout& layout);
  void draw(DrawList& list, const Menu& menu, const MenuLayout& layout,
            const MenuStyle& style) const;

  float page_in() const { return page_in_; }
  // The highlight's box now (it glides to the focused item's).
  Rect highlight() const { return glow_; }

 private:
  std::string page_;
  float page_in_ = 1;  // 0 when a page has just opened, 1 when it is in
  Rect glow_;          // the highlight's box now
  bool placed_ = false;
};

}  // namespace ge::ui
