// Draw strings (engine/hud/draw_strings): `draw` and `undraw` as typed, word for word, and the
// colour words `draw` reads.
//
// ge:summary Tests the draw and undraw commands' list and lines and the colour words
// ge:keywords test hud draw undraw draw-string colour macro
// ge:skill ui testing
#include "check.hpp"
#include "console/console.hpp"
#include "hud/draw_strings.hpp"

using namespace ge;

TEST(draw_lists_adds_and_replaces) {
  console::Console c;
  hud::DrawStrings s;
  hud::add_draw_commands(c, s);
  c.execute("draw");
  CHECK(c.output().back() == "No draw strings registered.");
  c.execute("draw fps 10 -20 red; draw fps 12 4; draw mine 1 2");
  CHECK(s.list.size() == 2 && s.list[0].x == 12 && s.list[0].ink == hud::DrawString::Ink::own);
  CHECK(c.cvars().find("mine") != nullptr);  // an unknown name made a setting to draw
  c.execute("draw");
  CHECK(c.output().back() == "mine               1    2");
  CHECK(c.output()[c.output().size() - 4] == "Name               X    Y");
  c.execute("draw a 1");
  CHECK(c.output().back() == "Usage: draw <name> <x> <y> [color]");
  c.execute("draw a 1 2 mauve");
  CHECK(c.output().back() == "Unknown color 'mauve'");
}

TEST(undraw_takes_one_or_all) {
  console::Console c;
  hud::DrawStrings s;
  hud::add_draw_commands(c, s);
  c.execute("draw one 1 1; draw two 2 2; undraw one; undraw three");
  CHECK(s.list.size() == 1 && s.list[0].name == "two");
  CHECK(c.output().back() == "Draw string 'three' not found.");
  c.execute("undraw all");
  CHECK(s.list.empty() && c.output().back() == "Deleted all draw strings.");
  c.execute("undraw");
  CHECK(c.output().back() == "Usage: undraw <name>");
}

TEST(colour_words) {
  hud::DrawString d;
  CHECK(hud::draw_color("green", d) && d.ink == hud::DrawString::Ink::color && d.color.g == 1);
  CHECK(hud::draw_color("#f00", d) && d.color.r == 1 && d.color.g == 0);
  CHECK(hud::draw_color("alt", d) && d.ink == hud::DrawString::Ink::alt);
  CHECK(!hud::draw_color("#zz0", d));
}
