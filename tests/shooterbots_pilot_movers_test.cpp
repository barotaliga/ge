// The pilot at a door (plugins/shooterbots/pilot_movers.hpp): whether a door's leaf stands across
// a way. A door sunk to within a step of the floor is walked over; one standing in the doorway is
// in the way; a jump down off a ledge over a door sunk level with the ledge (e1m4's trap room, its
// door lowered into the slime pit the bot jumps across) is not held by it, while a jump up past a
// door standing over the take-off still is.
//
// ge:summary Tests for the pilot's door check: sunk doors walked over and jumped from
// ge:keywords test pilot door leaf sunk lowered jump drop take-off leaf_in_way mover_in_way trap
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

}  // namespace
