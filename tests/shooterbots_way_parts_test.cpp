// The way through's shared parts (plugins/shooterbots/way_parts.hpp): the idle clock that starts
// the hopeful spell (every door taken as open for a while) runs only while he has nothing to do. A
// clock left running from an earlier spell set the doors open the first moment city1's bot fell
// off his graph on the way to a button, and he waited at the secret door the button opens instead.
// A step given up for good gets one more try from somewhere far from where it was given up, once
// (hangar1's secret wall, left twice from the pool under it while the climb out failed).
//
// ge:summary Tests for the way through's idle clock and the step given up and tried again
// ge:keywords test way_parts way_idle idle hopeful city1 way_forgive given-up hangar1
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

namespace {

ge::shooterbots::NavGraph two_places() {
  ge::shooterbots::NavGraph g;
  g.nodes.resize(3);
  g.nodes[0].origin = {1104, -816, 1128};  // hangar1's pool, under the wall
  g.nodes[1].origin = {1120, -800, 1128};  // beside it
  g.nodes[2].origin = {880, -464, 1432};   // the floor by the wall, out of the pool
  return g;
}

}  // namespace

TEST(a_step_given_up_is_tried_once_more_from_far_off_but_not_from_where_it_was_left) {
  const ge::shooterbots::NavGraph g = two_places();
  ge::shooterbots::WayStep st;
  st.left_until = ge::shooterbots::kWayForever;
  st.tries = ge::shooterbots::kWayTries;
  st.given_from = 0;
  CHECK(!wp::way_forgive(st, g, 1));  // still by the pool
  CHECK(st.left_until >= ge::shooterbots::kWayForever);
  CHECK(wp::way_forgive(st, g, 2));  // out on the floor
  CHECK(st.left_until < 0);
  CHECK(st.tries == ge::shooterbots::kWayTries - 1);  // one more leave gives it up again
  st.left_until = ge::shooterbots::kWayForever;
  st.given_from = 2;
  CHECK(!wp::way_forgive(st, g, 0));  // once only
}

TEST(a_step_only_left_a_while_or_given_up_from_nowhere_is_not_forgiven) {
  const ge::shooterbots::NavGraph g = two_places();
  ge::shooterbots::WayStep st;
  st.left_until = 100;  // left a while: its time comes back on its own
  st.given_from = 0;
  CHECK(!wp::way_forgive(st, g, 2));
  st.left_until = ge::shooterbots::kWayForever;
  st.given_from = -1;
  CHECK(!wp::way_forgive(st, g, 2));
  st.given_from = 0;
  CHECK(!wp::way_forgive(st, g, -1));
}
