// The fighter's climb out of the water at an edge (plugins/shooterbots/fighter.hpp): tried a few
// seconds, then the view is the aim's a while, then tried again. A hover sat on hangar1's lip over
// the bot's pool was never shot while the climb held the view on the edge for ten minutes.
//
// ge:summary Tests for the climb out of the water taking turns with the aim
// ge:keywords test fighter climb_turn climb_out hangar1 hover water edge
// ge:skill shooterbots testing
#include <cstdint>

#include "check.hpp"
#include "shooterbots/fighter.hpp"

namespace sb = ge::shooterbots;

TEST(the_climb_has_the_view_for_its_try_then_the_aim_for_its_rest) {
  double since = -1, rest = -1;
  int64_t yielded = 0;
  CHECK(sb::climb_turn(since, rest, 10, yielded));
  CHECK(since == 10);
  CHECK(sb::climb_turn(since, rest, 10 + sb::kClimbOutTry_s, yielded));
  CHECK(!sb::climb_turn(since, rest, 10.5 + sb::kClimbOutTry_s, yielded));  // the try is up
  CHECK(yielded == 1);
  CHECK(!sb::climb_turn(since, rest, 11 + sb::kClimbOutTry_s, yielded));  // the aim's rest
  CHECK(yielded == 1);
  const double back = 10.5 + sb::kClimbOutTry_s + sb::kClimbOutRest_s;
  CHECK(sb::climb_turn(since, rest, back, yielded));  // and a new try
  CHECK(since == back);
}
