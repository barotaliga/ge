// The way's traps while doors stay shut (plugins/shooterbots/way_through.hpp `way_shut_trap`): a
// closet a drop leads into, whose only way back to the main part is a door no touch opens, is a
// trap while that door is barred and none once it is not. On e1m4 the bot dropped into the ogres'
// closet over the slime pit to kill them and stood there for the last eight minutes: its door
// opens only from a trigger down in the pit.
//
// ge:summary Tests for the way's traps while doors stay shut: a closet behind a barred door
// ge:keywords test way_through way_shut_trap shuttrap trap closet barred door drop core e1m4
// ge:skill shooterbots testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "shooterbots/way_through.hpp"

namespace {

namespace sb = ge::shooterbots;

constexpr int32_t kDoor = 130;

// Nodes 0 and 1 the core (walked both ways), 1 drops to 2 (the closet), and 2 walks back to 0
// through the door.
sb::NavGraph closet() {
  sb::NavGraph g;
  g.nodes.resize(3);
  const auto link = [](int32_t to, sb::NavKind kind, int32_t mover) {
    sb::NavLink l;
    l.to = to;
    l.kind = kind;
    l.mover = mover;
    return l;
  };
  g.links = {link(1, sb::NavKind::walk, -1), link(0, sb::NavKind::walk, -1),
             link(2, sb::NavKind::drop, -1), link(0, sb::NavKind::walk, kDoor)};
  g.nodes[0].first_link = 0;
  g.nodes[0].links = 1;
  g.nodes[1].first_link = 1;
  g.nodes[1].links = 2;
  g.nodes[2].first_link = 3;
  g.nodes[2].links = 1;
  return g;
}

}  // namespace

TEST(a_closet_whose_only_way_out_is_a_barred_door_is_a_trap) {
  const sb::NavGraph g = closet();
  const std::vector<uint8_t> core{1, 1, 0};
  const std::vector<uint8_t> shut = sb::way_shut_trap(g, core, {kDoor});
  CHECK(shut[2] == 1);
  CHECK(shut[0] == 0 && shut[1] == 0);
}

TEST(the_closet_is_no_trap_once_its_door_is_not_barred) {
  const sb::NavGraph g = closet();
  const std::vector<uint8_t> core{1, 1, 0};
  const std::vector<uint8_t> open = sb::way_shut_trap(g, core, {});
  CHECK(open[0] == 0 && open[1] == 0 && open[2] == 0);
  // Another door barred: still a way back through this one.
  const std::vector<uint8_t> other = sb::way_shut_trap(g, core, {kDoor + 1});
  CHECK(other[2] == 0);
}
