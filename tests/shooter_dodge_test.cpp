// THE DODGE: an expert turns his side step back two or three times a second and seldom runs
// straight; a novice turns slowly and runs straight at his enemy half the time; a side the step
// would take off a ledge, into a wall or into water is never stepped to; the same seed dodges the
// same way.
//
// ge:summary Tests: the dodge's skill by trait, its stretches, and the side steps it refuses
// ge:keywords test dodge dodge_skill dodge_step dodge_clear ledge wall water side-step
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/dodge.hpp"
#include "shooter/hull.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;
using ge::test::loop::box;

namespace {

struct Run {
  int turns = 0, straight = 0, steps = 0;
};

// Ten seconds of dodging at 77 commands a second, both sides clear.
Run dodge_for(uint8_t trait, uint32_t seed) {
  sh::Dodge d;
  sh::dodge_init(d, seed);
  const sh::DodgeSkill k = sh::dodge_skill(trait);
  Run r;
  int was = 0;
  for (int i = 0; i < 770; i++) {
    const int dir = sh::dodge_step(d, k, i / 77.0, true, true);
    if (dir != was && was != 0 && dir != 0) r.turns++;
    if (dir == 0) r.straight++;
    r.steps++;
    was = dir;
  }
  return r;
}

}  // namespace

TEST(an_expert_turns_often_and_a_novice_runs_straight_more) {
  const Run expert = dodge_for(8, 7), novice = dodge_for(2, 7);
  CHECK(expert.turns >= 15);  // two or more turns a second over ten seconds
  CHECK(novice.turns < expert.turns);
  CHECK(novice.straight > expert.straight);
  CHECK(sh::dodge_skill(2).press < sh::dodge_skill(8).press);
  CHECK(sh::dodge_skill(10).press == 1);
  // The same seed dodges the same way.
  const Run again = dodge_for(8, 7);
  CHECK(again.turns == expert.turns && again.straight == expert.straight);
}

TEST(a_side_that_is_not_clear_is_never_stepped_to) {
  sh::Dodge d;
  sh::dodge_init(d, 3);
  const sh::DodgeSkill k = sh::dodge_skill(8);
  for (int i = 0; i < 400; i++) CHECK(sh::dodge_step(d, k, i / 77.0, false, true) >= 0);
  for (int i = 400; i < 800; i++) CHECK(sh::dodge_step(d, k, i / 77.0, true, false) <= 0);
  for (int i = 800; i < 900; i++) CHECK(sh::dodge_step(d, k, i / 77.0, false, false) == 0);
}

TEST(a_ledge_a_wall_and_water_are_not_clear) {
  Bench b;
  // The floor ends at x = 1024; a wall at y 200..232 above it; a pool of water at x -600..-400.
  sh::Brush water = box({-600, -1024, -64}, {-400, 1024, 0});
  water.contents = sh::kContentsWater;
  b.start({box({-1024, -1024, -64}, {-600, 1024, 0}), box({-400, -1024, -64}, {1024, 1024, 0}),
           box({-1024, 200, 0}, {1024, 232, 128}), box({-1024, -1024, -128}, {1024, 1024, -64}),
           water});
  // Standing at x 1000 facing +y (yaw 90): the right is +x, off the ledge; the left is clear.
  const sh::Vec3f edge{1000, 0, 24};
  CHECK(!sh::dodge_clear(b.sv, edge, 90, 1, -1));
  CHECK(sh::dodge_clear(b.sv, edge, 90, -1, -1));
  // Facing +x (yaw 0) beside the wall: the left is +y, into it.
  const sh::Vec3f by_wall{0, 170, 24};
  CHECK(!sh::dodge_clear(b.sv, by_wall, 0, -1, -1));
  CHECK(sh::dodge_clear(b.sv, by_wall, 0, 1, -1));
  // Beside the pool at x -380 facing +y: the left (-x) steps into the water.
  const sh::Vec3f by_pool{-380, 0, 24};
  CHECK(!sh::dodge_clear(b.sv, by_pool, 90, -1, -1));
  CHECK(sh::dodge_clear(b.sv, by_pool, 90, 1, -1));
}
