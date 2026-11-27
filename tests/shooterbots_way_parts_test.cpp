// The way through's shared parts (plugins/shooterbots/way_parts.hpp): the idle clock that starts
// the hopeful spell (every door taken as open for a while) runs only while he has nothing to do. A
// clock left running from an earlier spell set the doors open the first moment city1's bot fell
// off his graph on the way to a button, and he waited at the secret door the button opens instead.
//
// ge:summary Tests for the way through's idle clock
// ge:keywords test way_parts way_idle idle hopeful city1
// ge:skill shooterbots testing
#include "check.hpp"
#include "shooterbots/way_parts.hpp"

namespace wp = ge::shooterbots::way_parts;

TEST(the_idle_clock_starts_when_nothing_is_left_and_keeps_its_start) {
  CHECK(wp::way_idle(-1, false, 10) == 10);
  CHECK(wp::way_idle(10, false, 14) == 10);
}

TEST(something_to_do_stops_the_idle_clock_so_a_later_spell_starts_afresh) {
  double since = wp::way_idle(-1, false, 10);  // nothing to do at 10 s
  since = wp::way_idle(since, true, 12);       // a button to press from 12 s
  CHECK(since < 0);
  since = wp::way_idle(since, false, 200);  // off the graph a moment at 200 s
  CHECK(since == 200);                      // idle 0 s, not 190: no hopeful spell yet
}
