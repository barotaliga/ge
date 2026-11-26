// The crushers the nav avoids (plugins/shooterbots/map_nav.hpp `mark_crusher_harm`): a train that
// crushes and comes to rest for good pins whoever stands under it there, so the floor under that
// stop harms while its top stays the way on; one that only passes harms nothing. On e1m4 the
// crusher, shot down by two buttons, stays on its pit's floor and held a bot that cannot die
// there the rest of the map; e1m3's ceiling comes down and goes back up.
//
// ge:summary Tests for the crushers the nav avoids: the floor under a crusher's last stop
// ge:keywords test map_nav mark_crusher_harm crusher train dmg end_stop harm e1m4 e1m3
// ge:skill shooterbots testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "shooterbots/map_nav.hpp"

namespace {

namespace sb = ge::shooterbots;

// Node 0 on the pit's floor (origin 24 over it at 864), node 1 on the crusher's top where it
// rests (909), node 2 on the floor beside the pit.
sb::NavGraph pit() {
  sb::NavGraph g;
  g.nodes.resize(3);
  g.nodes[0].origin = {-250, 1600, 888};
  g.nodes[1].origin = {-250, 1600, 934};
  g.nodes[2].origin = {-100, 1600, 888};
  return g;
}

// A slab 28 high over the pit at 1317, its track down to 881 and back; `stays`: it stays at the
// bottom for good.
sb::NavMover crusher(bool stays) {
  sb::NavMover mv;
  mv.absmin = {-359, 1529, 1317};
  mv.absmax = {-153, 1743, 1345};
  mv.rest = {0, 0, 0};
  mv.stops = {{0, 0, 0}, {0, 0, -436}};
  mv.stop_wait_s = {0, 0};
  mv.dmg = 90;
  mv.end_stop = stays ? 1 : -1;
  return mv;
}

}  // namespace

TEST(the_floor_under_a_crusher_that_stays_down_harms_and_its_top_does_not) {
  const sb::NavGraph g = pit();
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_crusher_harm(g, {crusher(true)}, harm) == 1);
  CHECK(harm[0] == 1);
  CHECK(harm[1] == 0);
  CHECK(harm[2] == 0);
}

TEST(a_crusher_that_moves_on_or_does_no_harm_marks_nothing) {
  const sb::NavGraph g = pit();
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_crusher_harm(g, {crusher(false)}, harm) == 0);
  sb::NavMover soft = crusher(true);
  soft.dmg = 0;
  CHECK(sb::mark_crusher_harm(g, {soft}, harm) == 0);
  CHECK(harm[0] == 0 && harm[1] == 0 && harm[2] == 0);
}
