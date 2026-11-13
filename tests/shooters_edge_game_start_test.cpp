// A game started from SELECT GAME (games/shooters_edge/client/game_start.hpp): scr_conspeed in each
// game's own unit turned into the console's speed, a switched game's default taken only when the
// player kept the old one's, and the console held over the screen until the game is up.
//
// ge:summary Tests a game started from SELECT GAME: scr_conspeed per game, the console held, let go
// ge:keywords test shooters-edge game-start scr_conspeed con_speed loading console
// ge:skill console shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "shooters_edge/client/game_start.hpp"

namespace cl = shooters_edge::client;
namespace pr = shooters_edge::profiles;
namespace con = ge::console;

namespace {

const pr::GameProfile& sequel() { return *pr::find_game("sequel"); }

std::string value(con::Console& c, const char* name) { return c.cvars().find(name)->value; }

}  // namespace

TEST(scr_conspeed_is_lines_of_the_classic_screen_and_screens_in_the_sequel) {
  con::Console c;
  con::ConsoleView v(c);
  cl::GameStart s;
  cl::add_start_cvars(c, s, pr::classic_game());
  cl::start_apply_speed(c, s);
  CHECK(value(c, "scr_conspeed") == "300");
  CHECK(value(c, "con_speed") == "1.5");  // 300 lines a second of a 200-line screen
  c.cvars().set("scr_conspeed", "600", con::Source::local);
  cl::start_apply_speed(c, s);
  CHECK(value(c, "con_speed") == "3");
  // The player's own speed is kept across a game switched to.
  cl::start_switch_game(c, s, pr::classic_game(), sequel());
  cl::start_apply_speed(c, s);
  CHECK(value(c, "scr_conspeed") == "600");
  // The old game's default is not: the new game's own instead.
  c.cvars().set("scr_conspeed", "3", con::Source::local);
  cl::start_switch_game(c, s, sequel(), pr::classic_game());
  cl::start_apply_speed(c, s);
  CHECK(value(c, "scr_conspeed") == "300" && value(c, "con_speed") == "1.5");
}

TEST(the_console_is_held_down_until_the_game_is_up_then_slides_up) {
  con::Console c;
  con::ConsoleView v(c);
  cl::GameStart s;
  cl::add_start_cvars(c, s, pr::classic_game());
  cl::start_apply_speed(c, s);
  cl::start_begin(s, v, c, 10, {"Added packfile id1/pak0.pak"});
  CHECK(v.cover() == 1 && v.lines().back().text == "Added packfile id1/pak0.pak");
  CHECK(!cl::start_step(s, v, 10.2, true));  // up, but the lines have not shown long enough
  CHECK(!cl::start_step(s, v, 11, false));   // shown, but the game is not up
  CHECK(cl::start_step(s, v, 11, true));
  v.frame(0.2);  // a screen and a half a second
  CHECK(v.cover() > 0.69 && v.cover() < 0.71);
  v.frame(1);
  CHECK(v.cover() == 0);
  // A game that never comes up leaves no screen covered.
  cl::start_begin(s, v, c, 20, {});
  CHECK(cl::start_step(s, v, 20 + cl::kStartMost, false));
}
