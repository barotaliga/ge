// Tests: what the window draws round the sequel's bar (games/shooters_edge/client/sequel_bar.cpp):
// the bar's whole-number scale, the centre print's lines (centred, cut at 40 letters, placed at
// 35% of the screen or at 48 for a long message) and the crosshair left out when the packs hold
// no picture of it.
//
// ge:summary Tests: the sequel's centre print and crosshair round its bar, and the bar's scale
// ge:keywords test sequel centre-print crosshair status-bar scale add_sequel_screen
// ge:skill ui testing
// ge:test tests/shooters_edge_sequel_screen_test.cpp
#include <string>

#include "check.hpp"
#include "shooters_edge/client/sequel_bar.hpp"

namespace {

namespace cl = shooters_edge::client;

std::string placed(const cl::SequelBar& bar) {
  std::string s;
  for (const auto& o : bar.ops)
    s += "'" + o.text + "'@" + std::to_string(o.x) + "," + std::to_string(o.y) + " ";
  return s;
}

}  // namespace

TEST(the_bar_is_drawn_a_whole_number_larger_for_every_400_px_of_height) {
  CHECK(cl::sequel_bar_scale(300) == 1);
  CHECK(cl::sequel_bar_scale(480) == 1);
  CHECK(cl::sequel_bar_scale(800) == 2);
  CHECK(cl::sequel_bar_scale(1440) == 3);
}

TEST(a_centre_print_centres_each_line_at_35_percent_of_the_screen) {
  cl::SequelBar bar;
  cl::add_sequel_screen(bar, 0, "You need the key\nto open it", 640, 480);
  CHECK(placed(bar) == "'You need the key'@256,168 'to open it'@280,176 ");
}

TEST(a_long_centre_print_starts_at_48_and_a_line_is_cut_at_40_letters) {
  cl::SequelBar bar;
  cl::add_sequel_screen(bar, 0, "a\nb\nc\nd\n" + std::string(50, 'x'), 640, 480);
  CHECK(bar.ops.size() == 5);
  CHECK(bar.ops[0].y == 48);
  CHECK(bar.ops[4].y == 80);
  CHECK(bar.ops[4].text.size() == 40);
  CHECK(bar.ops[4].x == 160);
}

TEST(on_a_tall_window_the_centre_print_is_placed_on_the_smaller_screen) {
  cl::SequelBar bar;
  cl::add_sequel_screen(bar, 0, "hi", 1600, 1000);  // drawn twice over: an 800 x 500 screen
  CHECK(placed(bar) == "'hi'@392,175 ");
}

TEST(no_crosshair_is_drawn_when_the_packs_hold_no_picture_of_it) {
  cl::SequelBar bar;  // no files: every picture is missing
  cl::add_sequel_screen(bar, 1, "", 640, 480);
  CHECK(bar.ops.empty());
  cl::add_sequel_screen(bar, 0, "", 640, 480);
  CHECK(bar.ops.empty());
}
