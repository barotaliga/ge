// The pilot at a door (plugins/shooterbots/pilot_movers.hpp): whether a door's leaf stands across
// a way. A door sunk to within a step of the floor is walked over; one standing in the doorway is
// in the way; a jump down off a ledge over a door sunk level with the ledge (e1m4's trap room, its
// door lowered into the slime pit the bot jumps across) is not held by it, while a jump up past a
// door standing over the take-off still is. A platform that rises is waited for clear of its
// whole way; a ferry on the level is not. The route on or onto a mover is known (its floor unseen
// by the guards that read the static world), but not while it is waited for.
//
// ge:summary Tests for the pilot's door check: sunk doors walked over and jumped from
// ge:keywords test pilot door leaf sunk jump drop take-off leaf_in_way mover_in_way trap lift rises
// ge:keywords route_on_mover ferry
// ge:skill shooterbots testing
#include "check.hpp"
#include "shooterbots/pilot_movers.hpp"

namespace {

namespace sb = ge::shooterbots;
namespace pp = sb::pilot_parts;
using sb::Vec3f;

// e1m4's south door, lowered into the pit: its top 9 units over the trap room's floor (928).
constexpr Vec3f kSunkLo{641, -831, 795}, kSunkHi{895, -817, 937};
// The same door shut.
constexpr Vec3f kShutLo{641, -831, 929}, kShutHi{895, -817, 1071};

TEST(a_door_sunk_to_a_step_over_the_floor_is_walked_over) {
  const Vec3f a{680, -872, 952}, b{680, -840, 952};
  CHECK(!pp::leaf_in_way(a, b, false, kSunkLo, kSunkHi));
  CHECK(pp::leaf_in_way(a, b, false, kShutLo, kShutHi));
}

TEST(a_jump_down_off_the_ledge_over_a_sunk_door_is_not_held_by_it) {
  const Vec3f ledge{680, -840, 952}, far_floor{712, -648, 840};
  CHECK(!pp::leaf_in_way(ledge, far_floor, true, kSunkLo, kSunkHi));
  // Walked down instead, the way's floor is the lower end's: the leaf stands over it.
  CHECK(pp::leaf_in_way(ledge, far_floor, false, kSunkLo, kSunkHi));
  CHECK(pp::leaf_in_way(ledge, far_floor, true, kShutLo, kShutHi));
}

TEST(a_jump_up_past_a_door_standing_over_the_take_off_is_held) {
  const Vec3f low{712, -648, 840}, ledge{680, -840, 952};
  CHECK(pp::leaf_in_way(low, ledge, true, kSunkLo, kSunkHi));
}

TEST(a_platform_that_rises_is_waited_for_clear_of_its_whole_way_and_a_ferry_is_not) {
  // boss1's pillar up to the boss: built at its top stop, down 484 to its bottom one.
  sb::NavMover pillar;
  pillar.absmin = {-1128, -132, 1128};
  pillar.absmax = {-992, 4, 1624};
  pillar.rest = {1720, 832, 0};
  pillar.stops = {{1720, 832, 0}, {1720, 832, -484}};
  Vec3f lo, hi;
  CHECK(pp::rises(pillar, lo, hi));
  CHECK(lo.x == -1128 && hi.x == -992 && lo.z == 644 && hi.z == 1624);
  // A ferry between its stops on the level (boss1's to the button ledges) does not.
  sb::NavMover ferry;
  ferry.absmin = {-1156, 180, 1104};
  ferry.absmax = {-928, 246, 1120};
  ferry.stops = {{0, 0, 0}, {192, 0, 0}};
  CHECK(!pp::rises(ferry, lo, hi));
}

}  // namespace

TEST(the_route_on_or_onto_a_mover_is_known_and_a_wait_for_it_is_not) {
  // Three nodes: the ledge, a ferry's top, the ledge beyond (boss1's ferries over the lava).
  sb::NavGraph g;
  g.nodes.resize(3);
  g.nodes[1].mover = 368;
  sb::Pilot p;
  p.g = &g;
  p.route = {0, 1, 2};
  p.mode = sb::PilotMode::walk;
  p.k = 0;
  CHECK(sb::route_on_mover(p));  // onto it
  p.k = 1;
  CHECK(sb::route_on_mover(p));  // off it
  p.route = {0, 2};
  p.k = 0;
  CHECK(!sb::route_on_mover(p));  // no mover on the way
  p.route = {0, 1, 2};
  p.mode = sb::PilotMode::wait;  // waited for: the guards keep him on the ledge
  CHECK(!sb::route_on_mover(p));
}
