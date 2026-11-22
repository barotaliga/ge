// THE CONSOLE IN A CLASSIC GAME'S OWN LOOK: when the loaded game's packs carry its glyph sheet,
// the drop-down console (ui/console.hpp) is written in that sheet's letters (ui/charset_font.hpp's
// `sheet_font`) in the light greys the game wrote its console in, over the game's own console
// background (`conback`, the 320 x 200 picture sliding down with the panel, its foot at the
// panel's edge as the game drew it), or the picture Quake II's `con_background` names. With no
// sheet (the game's own content, or a game whose pictures are not read yet) the console keeps the
// engine's look.
//
// ge:summary The console in a classic game's own letters over its own background picture
// ge:keywords console classic conback conchars glyph-sheet console-background charset look
// ge:keywords classic-console draw_console sheet_font con_background
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "shooters_edge/menu/classic_art.hpp"
#include "ui/console.hpp"
#include "ui/font.hpp"
#include "ui/image.hpp"

namespace shooters_edge::menu {

// Kept in place: its style points at its own font.
struct ClassicConsole {
  ClassicConsole() = default;
  ClassicConsole(const ClassicConsole&) = delete;
  ClassicConsole& operator=(const ClassicConsole&) = delete;

  ge::ui::Font font;                    // the game's glyph sheet; not loaded: the engine's look
  ge::ui::ConsoleStyle style;           // its colours, `font` set when loaded
  const ge::ui::Image* back = nullptr;  // the game's console background, null: none
  // Quake II's `con_background`: another picture of the game's in its place ("" or `conback`: the
  // game's own), read by `read_back` the first frame it is named; one that does not read leaves
  // the game's own drawn.
  std::string back_name = "conback";
  std::function<bool(std::string_view name, ge::ui::Image* out)> read_back;

  bool on() const { return font.loaded(); }
  // The background drawn now: the named one once read, else `back`.
  const ge::ui::Image* background() const;

 private:
  mutable std::string read_name_ = "conback";  // the name `own_back_` was last read for
  mutable bool read_ok_ = false;
  mutable ge::ui::Image own_back_;
};

// The console's look from `art` (which must outlive `out`: the background is a view into it).
void classic_console(const ClassicArt& art, ClassicConsole& out);

// The console drawn: in `look`'s font and background when it is on, else the engine's own.
void draw_game_console(ge::ui::DrawList& list, const ge::console::ConsoleView& view,
                       const ClassicConsole& look, double t);

}  // namespace shooters_edge::menu
