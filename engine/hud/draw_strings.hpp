// DRAW STRINGS: a setting's or a macro's value kept on screen by the player (`draw cl_ups -60
// -40`, a clock with `draw com_time 8 8 alt`): Quake II's `draw` and `undraw`, any game's.
//
// - `draw <name> <x> <y> [colour]`: the name's VALUE drawn every frame (a macro's, else a
//   setting's; a name that is neither is made an empty setting, drawn once set). x and y are HUD
//   units (8 to a letter, scaled with the HUD); a negative x counts from the right edge and
//   right-aligns, a negative y counts from the bottom. Drawing a name again moves it. The colour:
//   black, red, green, yellow, blue, cyan, magenta, white (or 0..7 for them), `alt` (the HUD's
//   caption colour), `none` (the text's own), `#rgb`, `#rrggbb` or `#rrggbbaa`; anything else
//   "Unknown color 'x'".
// - `draw` alone lists them under `Name               X    Y`; "No draw strings registered." when
//   there are none.
// - `undraw <name>` removes one ("Draw string 'x' not found."), `undraw all` every one ("Deleted
//   all draw strings.").
//
// ge:summary Draw strings: a setting's or macro's value kept on screen (draw, undraw)
// ge:keywords draw undraw draw-string hud-text macro-on-screen clock speed cl_ups
// ge:skill ui console
// ge:test tests/hud_draw_strings_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "hud/hud.hpp"
#include "ui/draw.hpp"

namespace ge::hud {

struct DrawString {
  std::string name;
  int x = 0, y = 0;
  enum class Ink : uint8_t { own, alt, color } ink = Ink::own;
  ui::Color color{1, 1, 1, 1};
};

struct DrawStrings {
  std::vector<DrawString> list;  // in the order drawn first
};

// A colour word as `draw` reads it; false for none it knows.
bool draw_color(std::string_view word, DrawString& out);

// `draw` and `undraw` on `con` (`strings` must outlive it).
void add_draw_commands(console::Console& con, DrawStrings& strings);

// Every draw string's value now, on `frame`.
void draw_strings(ui::DrawList& dl, const Frame& frame, const Style& style,
                  const DrawStrings& strings, const console::Console& con);

}  // namespace ge::hud
