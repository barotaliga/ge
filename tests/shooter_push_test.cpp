// THE PUSHERS: a pusher moves on its own clock (`ltime`) only as far as its next think, then
// thinks, and a think that moves it pushes; a rider standing on it goes with it, as does
// anything it moves into; something pinned against the world blocks it: the pusher and all it
// moved go back and its blocked function runs with self the pusher and other the blocker; a
// point it meets is left where it is; a corpse (not solid, or a trigger) is flattened instead
// of blocking; something already stuck in another solid is not touched.
//
// ge:summary Tests: physics_pusher's clock and think, push_move's riders, blocks and corpses
// ge:keywords test push pusher push_move physics_pusher ltime rider blocked corpse plat door
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "shooter/physics.hpp"
#include "shooter/push.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;
using ge::test::loop::box;

namespace {

const sh::Vec3f kPlayerMins{-16, -16, -24}, kPlayerMaxs{16, 16, 32};

// A floor, a thick ceiling from z = 100 over x -200..200, and a plat (model 2) 128 square and 8
// high.
struct Room {
  Bench b;
  int plat = 0;
  int blocker = -1, blocks = 0;

  explicit Room(std::function<void(sh::Server&)> think = nullptr) {
    const sh::Func blocked = b.add([this](sh::Server& sv) {
      blocks++;
      blocker = sv.g.other;
      CHECK(sv.g.self == plat);
    });
    const sh::Func th = think ? b.add(std::move(think)) : sh::kNoFunc;
    b.start({box({-1024, -1024, -64}, {1024, 1024, 0}), box({-200, -200, 100}, {200, 200, 400})},
            {{box({-64, -64, 0}, {64, 64, 8})}});
    plat = sh::spawn_edict(b.sv);
    sh::Edict& p = b.ent(plat);
    p.solid = sh::kSolidBsp;
    p.movetype = sh::kMovetypePush;
    p.blocked = blocked;
    p.think = th;
    sh::set_brush_model(b.sv, plat, 2);
    sh::set_origin(b.sv, plat, {0, 0, 0});
  }

  int rider(sh::Vec3f origin, bool on_plat) {
    const int e =
        b.spawn_box(origin, kPlayerMins, kPlayerMaxs, sh::kSolidSlidebox, sh::kMovetypeStep);
    if (on_plat) {
      b.ent(e).flags = sh::kFlagOnGround;
      b.ent(e).groundentity = plat;
    }
    return e;
  }
};

}  // namespace

TEST(a_rider_goes_up_with_the_plat) {
  Room r;
  const int e = r.rider({0, 0, 32}, true);
  CHECK(sh::push_move(r.b.sv, r.plat, {0, 0, 20}));
  CHECK(r.b.ent(r.plat).origin.z == 20);
  CHECK(r.b.ent(e).origin.z == 52);
  CHECK((r.b.ent(e).flags & sh::kFlagOnGround) == 0);  // a non-player lifted is in the air
  // Walking players keep their ground.
  r.b.ent(e).movetype = sh::kMovetypeWalk;
  r.b.ent(e).flags = sh::kFlagOnGround;
  CHECK(sh::push_move(r.b.sv, r.plat, {0, 0, 10}));
  CHECK((r.b.ent(e).flags & sh::kFlagOnGround) != 0);
  CHECK(r.b.sv.counts.pushes == 2);
}

TEST(what_the_pusher_moves_into_is_pushed_along) {
  Room r;
  const int e = r.rider({100, 0, 24}, false);  // standing beside the plat, as high as it
  CHECK(sh::push_move(r.b.sv, r.plat, {30, 0, 0}));
  CHECK(r.b.ent(e).origin.x == 130);
  const int clear = r.rider({0, 300, 24}, false);  // out of its way
  CHECK(sh::push_move(r.b.sv, r.plat, {10, 0, 0}));
  CHECK(r.b.ent(clear).origin.x == 0);
}

TEST(a_rider_pinned_against_the_ceiling_blocks_and_everything_goes_back) {
  Room r;
  const int e = r.rider({0, 0, 32}, true);
  const int other = r.rider({100, 0, 24}, false);
  CHECK(sh::push_move(r.b.sv, r.plat, {0, 0, 20}));   // rider's top at 84
  CHECK(!sh::push_move(r.b.sv, r.plat, {0, 0, 30}));  // it would be at 114: pinned
  CHECK(r.b.ent(r.plat).origin.z == 20);
  CHECK(r.b.ent(e).origin.z == 52);
  CHECK(r.b.ent(other).origin.x == 100);
  CHECK(r.blocks == 1);
  CHECK(r.blocker == e);
  CHECK(r.b.sv.counts.pushes_blocked == 1);
}

TEST(a_point_is_left_where_it_is_and_a_corpse_is_flattened) {
  Room r;
  // A point just under the ceiling: the plat rises round it and would push it into the
  // ceiling, so it stays inside the plat and does not block it.
  const int point =
      r.b.spawn_box({0, 0, 99}, {0, 0, 0}, {0, 0, 0}, sh::kSolidBbox, sh::kMovetypeToss);
  CHECK(sh::push_move(r.b.sv, r.plat, {0, 0, 95}));
  CHECK(r.b.ent(point).origin.z == 99);
  CHECK(r.blocks == 0);

  Room c;
  const int corpse =
      c.b.spawn_box({0, 0, 32}, kPlayerMins, kPlayerMaxs, sh::kSolidTrigger, sh::kMovetypeToss);
  c.b.ent(corpse).flags = sh::kFlagOnGround;
  c.b.ent(corpse).groundentity = c.plat;
  CHECK(sh::push_move(c.b.sv, c.plat, {0, 0, 50}));  // its top would be at 114
  CHECK(c.blocks == 0);
  CHECK(c.b.ent(corpse).origin.z == 32);
  CHECK(c.b.ent(corpse).mins.x == 0 && c.b.ent(corpse).mins.y == 0);
  CHECK(c.b.ent(corpse).maxs.x == 0 && c.b.ent(corpse).maxs.z == -24);
}

TEST(a_box_already_stuck_elsewhere_is_left_alone) {
  Room r;
  const int stuck = r.rider({0, 0, 90}, true);  // its top in the ceiling already
  CHECK(sh::push_move(r.b.sv, r.plat, {0, 0, 10}));
  CHECK(r.b.ent(stuck).origin.z == 90);
}

TEST(a_pusher_moves_on_its_own_clock_only_as_far_as_its_next_think) {
  int thinks = 0;
  Room r([&](sh::Server&) { thinks++; });
  sh::Edict& p = r.b.ent(r.plat);
  p.velocity = {0, 0, 100};
  p.nextthink = 0.05f;  // on the pusher's clock
  sh::physics_pusher(r.b.sv, r.plat, 0.1);
  CHECK(thinks == 1);
  CHECK(std::fabs(r.b.ent(r.plat).origin.z - 5) < 1e-5f);
  CHECK_BITS(r.b.ent(r.plat).ltime, 0.05f);
  // No think ahead: it does not move at all.
  sh::physics_pusher(r.b.sv, r.plat, 0.1);
  CHECK(std::fabs(r.b.ent(r.plat).origin.z - 5) < 1e-5f);
  CHECK_BITS(r.b.ent(r.plat).ltime, 0.05f);
  // A think beyond the step: the whole step moves.
  r.b.ent(r.plat).nextthink = 1.0f;
  sh::physics_pusher(r.b.sv, r.plat, 0.1);
  CHECK(std::fabs(r.b.ent(r.plat).origin.z - 15) < 1e-4f);
  CHECK(thinks == 1);
}

TEST(a_think_that_moves_the_pusher_pushes_what_rides_it) {
  Room* room = nullptr;
  Room r([&](sh::Server& sv) {
    // Snap to the destination, as a mover arriving does.
    sh::set_origin(sv, room->plat, {0, 0, 30});
  });
  room = &r;
  const int e = r.rider({0, 0, 32}, true);
  sh::Edict& p = r.b.ent(r.plat);
  p.nextthink = 0.05f;
  p.velocity = {0, 0, 0};
  sh::physics_pusher(r.b.sv, r.plat, 0.1);
  CHECK(r.b.ent(r.plat).origin.z == 30);
  CHECK(r.b.ent(e).origin.z == 62);
}
