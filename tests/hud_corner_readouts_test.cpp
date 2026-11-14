// engine/hud/corner_readouts: the cvars take the game's defaults; the words of a speed, a time of
// day and a game's time; only the readouts turned on are drawn, stacked under what is above them.
//
// ge:summary Tests the corner readouts: show_speed, scr_clock, their words and stacking
// ge:keywords test show_speed scr_clock corner-readouts speed-words clock-words
// ge:skill ui testing
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "hud/corner_readouts.hpp"

namespace gh = ge::hud;
namespace con = ge::console;

TEST(the_cvars_take_the_games_defaults) {
  con::Console c;
  gh::ReadoutSettings s;
  s.clock = 2;  // a game whose match clock shows unless the player hides it
  gh::add_readout_cvars(c.cvars(), s);
  const con::Cvar* clock = c.cvars().find("scr_clock");
  const con::Cvar* speed = c.cvars().find("show_speed");
  CHECK(clock != nullptr && clock->fallback == "2" && s.clock == 2);
  CHECK(speed != nullptr && speed->fallback == "0");
  c.execute("show_speed 1; scr_clock 0");
  CHECK(s.speed == 1 && s.clock == 0);
  c.execute("show_speed 7");
  CHECK(s.speed == 2);
}

TEST(the_words_of_a_speed_and_the_times) {
  CHECK(gh::speed_words(319.6) == "320");
  CHECK(gh::speed_words(-3) == "0");
  CHECK(gh::time_of_day_words(7, 5) == "07:05");
  CHECK(gh::time_of_day_words(25, 61) == "23:59");
  CHECK(gh::game_time_words(0) == "0:00");
  CHECK(gh::game_time_words(723.9) == "12:03");
  CHECK(gh::game_time_words(3723) == "1:02:03");
}

TEST(only_the_readouts_turned_on_are_drawn_stacked) {
  const gh::Style style;
  const gh::Frame frame = gh::frame_for(1280, 720, style);
  gh::Readings r;
  r.ground_speed = 320;
  gh::ReadoutSettings s;
  ge::ui::DrawList none;
  none.begin(1280, 720);
  CHECK(gh::draw_corner_readouts(none, frame, style, s, r) == 0);
  s.speed = 1;
  ge::ui::DrawList one;
  one.begin(1280, 720);
  const float h1 = gh::draw_corner_readouts(one, frame, style, s, r);
  s.clock = 1;
  ge::ui::DrawList two;
  two.begin(1280, 720);
  const float h2 = gh::draw_corner_readouts(two, frame, style, s, r, 40);
  CHECK(h1 > 0 && h2 > h1 * 1.5f);
}
