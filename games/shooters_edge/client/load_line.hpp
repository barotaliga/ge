// A CLASSIC GAME LOADS A MAP IN ITS CONSOLE, as the classic games did: the console held down over
// the screen and its last line "Loading base1... 3.6s", rewritten as the time goes (the spinner
// and the title's night are Shooter's Edge's own). The line and its hold, apart from the loader
// (map_load.hpp) so the window's rule is tested without a GPU: the hold lets go once the load is
// done, or at once when the world shown is another's (a server joined, a demo played). The map
// loading behind the title then waits, untaken, until the window is back, and its line would
// otherwise cover the game joined for as long as it stays there.
//
// ge:summary The console held down while a map loads, and let go once the world is another's
// ge:keywords load-line console-load loading-console held-console join-while-loading con_full
// ge:skill shooter ui
// ge:test tests/shooters_edge_q2_join_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "console/view.hpp"

namespace shooters_edge::client {

struct LoadLine {
  std::string what;       // the map the line is for
  uint64_t n = 0;         // the line's number in the scrollback
  bool held = false;      // the console is held down for it
  bool given_up = false;  // let go for another world: not held again until this load is done
};

// While `what` loads (begun at `since_s`): the line printed or rewritten on `con`/`view` at
// `now_s` and the console held.
void hold_load_line(std::string_view what, double since_s, LoadLine& line,
                    ge::console::Console& con, ge::console::ConsoleView& view, double now_s);

// The console the line held let go once nothing loads (`loading` false) or the world shown is
// another's (`elsewhere`: a server joined or a demo played), unless `keep` (a game's start or a
// demo's first frames still hold it). Let go for another world, the line is not held again for
// the same load (`given_up`): a join's reconnect leaves no stale line behind.
void release_load_line(bool loading, bool elsewhere, LoadLine& line, ge::console::ConsoleView& view,
                       bool keep);

}  // namespace shooters_edge::client
