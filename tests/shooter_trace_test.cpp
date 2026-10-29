// THE WORLD'S TRACE: the world first, then every solid box or brush the move's bounds reach; the
// hull a brush is met with chosen by the mover's width; a box entity met as a box grown by the
// mover; the trace's own entity, its owner and what it owns skipped; triggers never block; a
// no-monsters trace meets brushes only; a missile meets a monster as a 30-unit cube; a point
// passes through a point-sized entity met by a sized mover's trace; a position test answers
// whether a box is stuck.
//
// ge:summary Tests: trace_move's hulls, boxes, skips, the missile cube and the position test
// ge:keywords test trace trace_move clip_to_entity hull-choice passent owner missile monster
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "shooter/trace.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;

namespace {

bool near(float a, float b, float tol = 0.05f) { return std::fabs(a - b) <= tol; }

const sh::Vec3f kZero{0, 0, 0};

}  // namespace

TEST(the_world_is_met_with_the_hull_the_movers_width_picks) {
  Bench b;
  b.start();
  const sh::HullTrace point =
      sh::trace_move(b.sv, {0, 0, 100}, kZero, kZero, {0, 0, -100}, sh::kTraceNormal, -1);
  CHECK(point.ent == 0);
  CHECK(near(point.endpos.z, 0));
  CHECK(point.plane.normal.z == 1);
  // A player's box (32 wide): hull 1, standing 24 above the floor.
  const sh::HullTrace player = sh::trace_move(b.sv, {0, 0, 100}, {-16, -16, -24}, {16, 16, 32},
                                              {0, 0, -100}, sh::kTraceNormal, -1);
  CHECK(near(player.endpos.z, 24));
  // A 64-wide box: hull 2, whose box is 24 below the origin too.
  const sh::HullTrace large = sh::trace_move(b.sv, {0, 0, 100}, {-32, -32, -24}, {32, 32, 64},
                                             {0, 0, -100}, sh::kTraceNormal, -1);
  CHECK(near(large.endpos.z, 24));
  // A 2-wide box is met as a point.
  const sh::HullTrace thin = sh::trace_move(b.sv, {0, 0, 100}, {-1, -1, -1}, {1, 1, 1},
                                            {0, 0, -100}, sh::kTraceNormal, -1);
  CHECK(near(thin.endpos.z, 1));
}

TEST(a_box_entity_blocks_as_a_box_grown_by_the_mover) {
  Bench b;
  b.start();
  const int box =
      b.spawn_box({100, 0, 32}, {-16, -16, -16}, {16, 16, 16}, sh::kSolidBbox, sh::kMovetypeNone);
  const sh::HullTrace point =
      sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {200, 0, 32}, sh::kTraceNormal, -1);
  CHECK(point.ent == box);
  CHECK(near(point.endpos.x, 84));
  const sh::HullTrace sized =
      sh::trace_move(b.sv, {0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, {200, 0, 32}, sh::kTraceNormal, -1);
  CHECK(near(sized.endpos.x, 76));
  // A trigger never blocks.
  b.ent(box).solid = sh::kSolidTrigger;
  sh::link_edict(b.sv, box, false);
  const sh::HullTrace through =
      sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {200, 0, 32}, sh::kTraceNormal, -1);
  CHECK(through.fraction == 1 && through.ent == -1);
}

TEST(the_trace_skips_itself_its_owner_and_what_it_owns) {
  Bench b;
  b.start();
  const int box =
      b.spawn_box({100, 0, 32}, {-16, -16, -16}, {16, 16, 16}, sh::kSolidBbox, sh::kMovetypeNone);
  const int mover = b.spawn_box({0, 0, 32}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeNone);
  auto run = [&](int pass) {
    return sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {200, 0, 32}, sh::kTraceNormal, pass);
  };
  CHECK(run(box).fraction == 1);
  CHECK(run(mover).ent == box);
  b.ent(box).owner = mover;
  CHECK(run(mover).fraction == 1);
  b.ent(box).owner = 0;
  b.ent(mover).owner = box;
  CHECK(run(mover).fraction == 1);
}

TEST(a_no_monsters_trace_meets_brushes_only) {
  Bench b;
  b.start();
  b.spawn_box({100, 0, 32}, {-16, -16, -16}, {16, 16, 16}, sh::kSolidSlidebox, sh::kMovetypeNone);
  const sh::HullTrace tr =
      sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {200, 0, 32}, sh::kTraceNoMonsters, -1);
  CHECK(tr.fraction == 1);
  const sh::HullTrace down =
      sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {0, 0, -32}, sh::kTraceNoMonsters, -1);
  CHECK(down.ent == 0);
}

TEST(a_missile_meets_a_monster_as_a_30_unit_cube) {
  Bench b;
  b.start();
  const int monster = b.spawn_box({100, 0, 32}, {-16, -16, -16}, {16, 16, 16}, sh::kSolidSlidebox,
                                  sh::kMovetypeStep);
  b.ent(monster).flags = sh::kFlagMonster;
  sh::link_edict(b.sv, monster, false);
  const sh::HullTrace missile =
      sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {200, 0, 32}, sh::kTraceMissile, -1);
  CHECK(missile.ent == monster);
  CHECK(near(missile.endpos.x, 69));
  // It passes a monster's side that a point would miss by less than the cube.
  const sh::HullTrace grazing =
      sh::trace_move(b.sv, {0, 28, 32}, kZero, kZero, {200, 28, 32}, sh::kTraceMissile, -1);
  CHECK(grazing.ent == monster);
  const sh::HullTrace point =
      sh::trace_move(b.sv, {0, 28, 32}, kZero, kZero, {200, 28, 32}, sh::kTraceNormal, -1);
  CHECK(point.fraction == 1);
  // A box that is not a monster is met as itself.
  b.ent(monster).flags = 0;
  const sh::HullTrace plain =
      sh::trace_move(b.sv, {0, 0, 32}, kZero, kZero, {200, 0, 32}, sh::kTraceMissile, -1);
  CHECK(near(plain.endpos.x, 84));
}

TEST(a_sized_movers_trace_passes_point_sized_entities) {
  Bench b;
  b.start();
  b.spawn_box({100, 0, 32}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeNone);
  const int mover =
      b.spawn_box({0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  const sh::HullTrace tr = sh::trace_move(b.sv, {0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, {200, 0, 32},
                                          sh::kTraceNormal, mover);
  CHECK(tr.fraction == 1);
}

TEST(the_position_test_answers_whether_a_box_is_stuck) {
  Bench b;
  b.start();
  const int e = b.spawn_box({0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  CHECK(!sh::test_entity_position(b.sv, e));
  sh::set_origin(b.sv, e, {0, 0, 4});
  CHECK(sh::test_entity_position(b.sv, e));
  const int other =
      b.spawn_box({300, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  sh::set_origin(b.sv, e, {304, 0, 32});
  CHECK(sh::test_entity_position(b.sv, e));
  (void)other;
}
