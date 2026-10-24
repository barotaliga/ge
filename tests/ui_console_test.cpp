// The drop-down console drawn: closed it draws only the notify lines; down, the panel covers the
// view's share of the screen in con_bgtop to con_bgbottom with the edge under it, the input line
// holds the prompt, the hint and the cursor, and the suggestions hang under the edge.
//
// ge:summary Tests the console's draw list: notify, panel and gradient, input, hint, suggestions
// ge:keywords test ui console draw panel gradient notify suggestion hint cursor
// ge:skill ui console testing
#include <string>

#include "check.hpp"
#include "console/view.hpp"
#include "ui/console.hpp"
#include "ui/raster.hpp"

using namespace ge;

namespace {

ui::Image drawn(const console::ConsoleView& v, int w = 640, int h = 360, double t = 0) {
  ui::DrawList dl;
  dl.begin(w, h);
  ui::draw_console(dl, v, {}, nullptr, t);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

int channel(const ui::Image& img, int x, int y, int c) {
  return img.rgba[(static_cast<size_t>(y) * img.width + x) * 4 + c];
}

bool has_text_run(const ui::DrawList& dl, const std::string& text) {
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::text && c.text.text == text) return true;
  return false;
}

}  // namespace

TEST(closed_it_draws_only_the_notify_lines) {
  console::Console c;
  console::ConsoleView v(c);
  ui::DrawList dl;
  dl.begin(640, 360);
  ui::draw_console(dl, v);
  CHECK(dl.size() == 0);
  v.add(console::LineKind::chat, "Ada: hi");
  dl.begin(640, 360);
  ui::draw_console(dl, v);
  CHECK(dl.size() == 2 && has_text_run(dl, "Ada: hi"));
}

TEST(down_the_panel_is_the_gradient_over_its_share_of_the_screen) {
  console::Console c;
  console::ConsoleView v(c);
  c.execute("con_bgtop 400000; con_bgbottom 000040; con_bgalpha 1; con_height 0.5");
  v.toggle();
  v.frame(10);
  const ui::Image img = drawn(v);
  CHECK(channel(img, 320, 2, 0) > 50 && channel(img, 320, 2, 2) < 10);      // red at the top
  CHECK(channel(img, 320, 170, 2) > 50 && channel(img, 320, 170, 0) < 10);  // blue at the foot
  CHECK(channel(img, 320, 300, 0) == 0 && channel(img, 320, 300, 2) == 0);  // the game below
}

TEST(the_input_line_holds_the_prompt_and_the_hint) {
  console::Console c;
  console::ConsoleView v(c);
  c.cvars().add({.name = "sv_deathmatch", .fallback = "0"});
  v.toggle();
  v.frame(10);
  v.type("/sv_dea");
  ui::DrawList dl;
  dl.begin(640, 360);
  ui::draw_console(dl, v);
  CHECK(has_text_run(dl, "] /sv_dea") && has_text_run(dl, "thmatch"));
  CHECK(has_text_run(dl, "sv_deathmatch") && has_text_run(dl, "= 0"));
  // The list hangs under the panel's edge.
  float list_top = 1e9f;
  for (const ui::Cmd& cmd : dl.cmds())
    if (cmd.kind == ui::CmdKind::text && cmd.text.text == "sv_deathmatch")
      list_top = cmd.text.baseline;
  CHECK(list_top > 180);
}

TEST(the_cursor_blinks) {
  console::Console c;
  console::ConsoleView v(c);
  v.toggle();
  v.frame(10);
  ui::DrawList on, off;
  on.begin(640, 360);
  off.begin(640, 360);
  ui::draw_console(on, v, {}, nullptr, 0.1);
  ui::draw_console(off, v, {}, nullptr, 0.6);
  CHECK(on.size() == off.size() + 1);
}
