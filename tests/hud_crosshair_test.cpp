// engine/hud/crosshair: the cvars take the game's settings as their defaults and write them; the
// built marks are painted and only 2..7 are; a colour is a palette index, a grey without a
// palette, or "R G B", and anything else leaves the mark untinted; the picture is the built mark
// or the game's own at its density, sized and faded, and nothing shows for mark 0, alpha 0 or a
// game with no mark of its own.
//
// ge:summary Tests the crosshair: cvars and a game's defaults, marks, colours, the picture
// ge:keywords test crosshair crosshaircolor crosshairsize crosshairalpha crosshair-marks defaults
// ge:skill ui testing
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "hud/crosshair.hpp"

namespace gh = ge::hud;
namespace ui = ge::ui;
namespace con = ge::console;

TEST(the_cvars_take_the_games_settings_as_their_defaults) {
  con::Console c;
  gh::CrosshairSettings s;
  s.mark = 4;  // a game whose crosshair is the open cross unless the player says otherwise
  s.size = 1.5;
  gh::add_crosshair_cvars(c.cvars(), s);
  const auto fallback = [&](const char* name) {
    const con::Cvar* v = c.cvars().find(name);
    return v != nullptr ? v->fallback : std::string("(none)");
  };
  const con::Cvar* mark = c.cvars().find("crosshair");
  CHECK(mark != nullptr && mark->is_default());
  CHECK(fallback("crosshair") == "4");
  CHECK(fallback("crosshairsize") == "1.5");
  CHECK(fallback("crosshairalpha") == "1");
  c.execute("crosshair 6; crosshaircolor \"255 0 51\"; crosshairsize 2; crosshairalpha 0.5");
  CHECK(s.mark == 6 && s.colour == "255 0 51" && s.size == 2 && s.alpha == 0.5);
  c.execute("crosshair 9");  // past the last mark: held to it
  CHECK(s.mark == gh::kCrosshairMarks);
  c.execute("reset crosshair");
  CHECK(s.mark == 4);
}

TEST(the_size_reaches_from_a_tenth_to_nine_times) {
  con::Console c;
  gh::CrosshairSettings s;
  gh::add_crosshair_cvars(c.cvars(), s);
  c.execute("crosshairsize 9");  // Quake II's ch_scale reaches nine times
  CHECK(s.size == 9);
  c.execute("crosshairsize 0.1");
  CHECK(s.size == 0.1);
  c.execute("crosshairsize 12");  // past the end: held to it
  CHECK(s.size == 9);
}

TEST(only_the_built_marks_are_painted) {
  for (int32_t m = 2; m <= gh::kCrosshairMarks; ++m) CHECK(!gh::crosshair_mark(m).empty());
  CHECK(gh::crosshair_mark(0).empty() && gh::crosshair_mark(1).empty());
  CHECK(gh::crosshair_mark(gh::kCrosshairMarks + 1).empty());
}

TEST(a_colour_is_a_palette_index_a_grey_or_red_green_and_blue) {
  std::vector<uint8_t> palette(768, 0);
  palette[size_t{12} * 3] = 255;  // index 12 is red in this palette
  const ui::Color red = gh::crosshair_colour("12", palette);
  CHECK(red.r == 1 && red.g == 0 && red.b == 0 && red.a == 1);
  CHECK(gh::crosshair_colour("999", palette).r == palette[size_t{255} * 3] / 255.0f);
  CHECK(gh::crosshair_colour("51", {}).g == 0.2f);  // no palette: a grey of that level
  const ui::Color rgb = gh::crosshair_colour(" 255 0  51 ", palette);
  CHECK(rgb.r == 1 && rgb.g == 0 && rgb.b == 0.2f);
  for (const char* white : {"", "pink", "1 2", "1 2 3 4", "-3"}) {
    const ui::Color w = gh::crosshair_colour(white, palette);
    CHECK(w.r == 1 && w.g == 1 && w.b == 1 && w.a == 1);
  }
}

TEST(the_picture_is_the_built_mark_or_the_games_own) {
  ui::Image own;
  own.width = own.height = 16;
  own.rgba.assign(size_t{16} * 16 * 4, 255);
  std::vector<uint8_t> palette(768, 0);
  palette[size_t{7} * 3 + 1] = 255;  // index 7 is green in this palette
  gh::CrosshairSettings s;
  s.mark = 1;
  s.colour = "7";
  s.size = 2;
  bool shown = false;
  const gh::Picture p = gh::crosshair_picture(s, own, 3.0f, palette, &shown);
  CHECK(shown && p.image == &own && p.density == 1.5f);
  CHECK(p.tint.r == 0 && p.tint.g == 1 && p.tint.b == 0 && p.tint.a == 1);
  s.mark = 5;
  s.alpha = 0.5;
  const gh::Picture built = gh::crosshair_picture(s, own, 3.0f, palette, &shown);
  CHECK(shown && built.image == &gh::crosshair_mark(5));
  CHECK(built.density == gh::kMarkDensity / 2 && built.tint.a == 0.5f);
  s.mark = 0;
  gh::crosshair_picture(s, own, 3.0f, palette, &shown);
  CHECK(!shown);
  s.mark = 1;
  s.alpha = 0;
  gh::crosshair_picture(s, own, 3.0f, palette, &shown);
  CHECK(!shown);
  s.alpha = 1;
  const ui::Image empty;
  gh::crosshair_picture(s, empty, 3.0f, palette, &shown);
  CHECK(!shown);  // a game with no mark of its own draws none at 1
}

TEST(cl_crossx_and_cl_crossy_move_it_and_a_game_may_shade_or_colour_it_by_health) {
  con::Console c;
  gh::CrosshairSettings s;
  gh::add_crosshair_cvars(c.cvars(), s);
  c.execute("cl_crossx 12; cl_crossy -4.5");
  CHECK(s.offset[0] == 12 && s.offset[1] == -4.5);
  // Health: white at full, yellow at two thirds, red at a third, black dead.
  const auto same = [](ui::Color a, float r, float g, float b) {
    return a.r == r && std::fabs(a.g - g) < 1e-6f && std::fabs(a.b - b) < 1e-6f;
  };
  CHECK(same(gh::crosshair_health(100), 1, 1, 1) && same(gh::crosshair_health(66), 1, 1, 0));
  CHECK(same(gh::crosshair_health(33), 1, 0, 0) && same(gh::crosshair_health(0), 0, 0, 0));
  CHECK(same(gh::crosshair_health(50), 1, 17 / 33.0f, 0));
  bool shown = false;
  s.mark = 2;
  s.by_health = true;
  CHECK(same(gh::crosshair_picture(s, {}, 1, {}, &shown, 40).tint, 1, 7 / 33.0f, 0) && shown);
  s.by_health = false;
  s.shade[1] = 0.5;  // a game's half of green
  CHECK(same(gh::crosshair_picture(s, {}, 1, {}, &shown).tint, 1, 0.5f, 1));
}
