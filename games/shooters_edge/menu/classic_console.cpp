// The console in a classic game's own look (classic_console.hpp).
//
// ge:summary Builds the classic console's font, colours and background from the game's pictures
// ge:keywords console classic conback conchars glyph-sheet console-background
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/classic_console.hpp"

#include "ui/charset_font.hpp"

namespace shooters_edge::menu {

namespace ui = ge::ui;

void classic_console(const ClassicArt& art, ClassicConsole& out) {
  out.font = ui::Font();
  out.style = {};
  out.back = nullptr;
  if (art.glyphs.empty() || !ui::sheet_font(art.glyphs, out.font)) return;
  ui::ConsoleStyle& st = out.style;
  st.font = &out.font;
  // The game's console: its lines in the sheet's own light grey, what the player typed and the
  // game's notices a little brighter, chat in the sheet's colours a shade warmer.
  st.kinds = {ui::hex(0xd8d8d8), ui::hex(0xffffff), ui::hex(0xf0e0b0), ui::hex(0xb8d8f0),
              ui::hex(0xf0c8f0), ui::hex(0xe0e8c8), ui::hex(0xffffff), ui::hex(0xffe090)};
  st.input = ui::hex(0xffffff);
  st.hint = ui::hex(0x8c7c64);
  st.cursor = ui::hex(0xe8d8b0);
  st.edge = ui::hex(0x8c5c2c);
  st.list_fill = ui::hex(0x1c140c, 0.94f);
  st.list_lit = ui::hex(0x5c3c1c);
  st.list_name = ui::hex(0xe8dcc8);
  st.list_detail = ui::hex(0x9c8c74);
  st.notify_band = ui::hex(0x000000, 0.0f);  // the game wrote its notices straight on the view
  st.image_focus_y = 1;                      // the picture slides down with the panel
  out.back = art.find("conback");
}

void draw_game_console(ui::DrawList& list, const ge::console::ConsoleView& view,
                       const ClassicConsole& look, double t) {
  if (look.on())
    ui::draw_console(list, view, look.style, look.back, t);
  else
    ui::draw_console(list, view, {}, nullptr, t);
}

}  // namespace shooters_edge::menu
