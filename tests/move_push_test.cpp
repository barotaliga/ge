// What moves a body from outside its own step (engine/move/push.hpp) and the surfaces that do
// something (world.hpp's surface fields), held to M28c's promises: a body rides a moving platform
// without sliding off it and lands back on it after a jump; a lift carries its rider up and the
// rider walks off at the top; a pusher shoves a body and a crush is refused; a knock-back leaves
// the ground and spends the coyote time; a jump pad's arc lands on its target at its apex; a
// trampoline bounces a landing until what it gives back is too little; a ladder is climbed, left
// at its top, climbed down looking down, jumped off along its normal, and backed away from at its
// foot.
//
// ge:summary Tests: moving ground, lifts, pushers, knock-back, jump pads, trampolines, ladders
// ge:keywords test move push carry launch arc_velocity platform lift pusher crush knockback
// ge:keywords jump-pad trampoline bounce ladder climb
// ge:skill move testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "move/box_world.hpp"
#include "move/push.hpp"
#include "move/scenarios.hpp"

namespace mv = ge::move;

namespace {

constexpr double kHz = 120;
constexpr double kDt = 1.0 / kHz;

struct Last {
  mv::MoveState s;
  int ladder_on = 0, ladder_off = 0, bounces = 0, landings = 0;
  double first_bounce = 0, last_bounce = 0;
  static void on(void* ctx, const mv::MoveState& s, const mv::MoveCmd&, const mv::MoveEvents& ev) {
    auto* l = static_cast<Last*>(ctx);
    l->s = s;
    if (ev.has(mv::kLadderOn)) l->ladder_on++;
    if (ev.has(mv::kLadderOff)) l->ladder_off++;
    if (ev.has(mv::kLanded)) l->landings++;
    if (ev.has(mv::kBounced)) {
      if (l->bounces == 0) l->first_bounce = ev.impact;
      l->last_bounce = ev.impact;
      l->bounces++;
    }
  }
};

Last run(const char* id) {
  const mv::MoveScenario* sc = mv::find_move_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  Last l;
  if (sc == nullptr) return l;
  mv::run_move_scenario(*sc, *mv::find_preset(sc->preset), kHz, Last::on, &l);
  return l;
}

const mv::MoveSpec& classic() { return *mv::find_preset("classic_arena"); }
double u() { return classic().unit; }

}  // namespace

TEST(a_body_rides_a_moving_platform_and_lands_back_where_it_left) {
  // The platform slides at 2 m/s for 3 s from z = 0: the body standing at its middle ends there.
  const Last l = run("platform-ride");
  CHECK(l.s.grounded && l.landings == 1);
  const double platform_z = 2 * 3;  // m it travelled
  CHECK_MSG(std::fabs(l.s.pos.z / u() - platform_z) < 0.02,
            "the body is " + std::to_string(l.s.pos.z / u() - platform_z) + " m off its spot");
  CHECK(l.s.ground_solid == 2);        // standing on the platform, not the pit's floor
  CHECK(std::fabs(l.s.vel.z) < 1e-9);  // its own velocity is relative to the platform
}

TEST(a_lift_carries_its_rider_up_and_the_rider_walks_off_at_the_top) {
  const Last l = run("lift");
  CHECK(l.s.grounded);
  CHECK(std::fabs(l.s.pos.y - 3 * u()) < 0.1);
  CHECK(l.s.pos.z > 3 * u());  // walked off onto the ledge
}

TEST(a_pusher_shoves_a_body_and_a_crush_is_refused) {
  const Last l = run("pusher");
  // The wall's face starts at 3 m and comes on 4.5 m: the body's front ends at its face.
  const double face = 3 - 1.5 * 3;
  CHECK_MSG(std::fabs((l.s.pos.z + classic().radius) / u() - face) < 0.05,
            std::to_string(l.s.pos.z / u()));
  CHECK(l.s.grounded);

  // Against a second wall there is nowhere to go: carry says so and leaves the body.
  mv::BoxWorld w;
  mv::add_box(w, {-500, -32, -500}, {500, 0, 500});
  mv::add_box(w, {-100, 0, -100}, {100, 100, -17});  // just behind the body
  const size_t wall = mv::add_box(w, {-100, 0, 16.5}, {100, 100, 60});
  const mv::World world = mv::as_world(w);
  mv::MoveState s = mv::stand_at(classic(), world, {0, 0, 0});
  mv::move_brush(w, wall, {0, 0, -4}, kDt);
  const mv::Vec3 before = s.pos;
  CHECK(!mv::carry(classic(), world, s, static_cast<uint32_t>(wall + 1), {0, 0, -4}));
  CHECK(s.pos.z == before.z);
}

TEST(a_knock_back_leaves_the_ground_and_spends_the_coyote_time) {
  mv::MoveSpec spec = classic();
  spec.coyote_time = 0.2;
  mv::BoxWorld w;
  mv::add_box(w, {-5000, -32, -5000}, {5000, 0, 5000});
  const mv::World world = mv::as_world(w);
  mv::MoveState s = mv::stand_at(spec, world, {0, 0, 0});
  mv::push(s, {0, 200, 300});
  CHECK(!s.grounded);
  mv::MoveCmd jump;
  jump.jump = true;
  const mv::MoveEvents ev = mv::step_move(spec, world, s, jump, kDt);
  CHECK(!ev.has(mv::kJumped));  // no coyote jump off a knock
  const Last l = run("knockback");
  CHECK(l.landings == 1 && l.s.grounded);
}

TEST(a_jump_pads_arc_lands_on_its_target_at_its_apex) {
  for (const double hz : {60.0, 120.0, 250.0}) {
    mv::BoxWorld w;
    mv::add_box(w, {-5000, -32, -5000}, {5000, 0, 5000});
    const mv::World world = mv::as_world(w);
    mv::MoveState s = mv::stand_at(classic(), world, {0, 0, 0});
    const mv::Vec3 target{0, 0, 400};
    mv::launch(classic(), s, mv::arc_velocity(s.pos, target, 100, classic().gravity));
    double apex = 0;
    for (int i = 0; i < 4 * hz && !s.grounded; i++) {
      mv::step_move(classic(), world, s, {}, 1 / hz);
      if (s.pos.y > apex) apex = s.pos.y;
    }
    CHECK(s.grounded);
    CHECK_MSG(std::fabs(s.pos.z - target.z) < 400.0 / hz, std::to_string(s.pos.z));
    CHECK_MSG(std::fabs(apex - 100) < 0.5, std::to_string(apex));
  }
  const Last l = run("jump-pad");
  CHECK(l.s.grounded && std::fabs(l.s.pos.y - 3 * u()) < 0.1);
}

TEST(a_trampoline_bounces_until_it_gives_back_too_little) {
  const Last l = run("trampoline");
  CHECK_MSG(l.bounces == 3, std::to_string(l.bounces) + " bounces");
  CHECK(l.landings == 1 && l.s.grounded);
  CHECK(l.last_bounce < l.first_bounce);
  // The landing that stuck gave back less than unground_speed.
  CHECK(l.last_bounce * 0.8 * 0.8 < classic().unground_speed + 1e-9);
}

TEST(a_ladder_is_climbed_and_left_at_its_top) {
  const Last l = run("ladder");
  CHECK(l.ladder_on == 1 && l.ladder_off == 1);
  CHECK(l.s.grounded && std::fabs(l.s.pos.y - 4 * u()) < 0.1);
  CHECK(l.s.pos.z > 3 * u());
}

TEST(a_ladder_is_climbed_down_looking_down_jumped_off_and_backed_away_from) {
  const mv::MoveSpec& spec = classic();
  mv::BoxWorld w;
  mv::add_box(w, {-5000, -32, -5000}, {5000, 0, 5000});
  mv::add_box(w, {-100, 0, 100}, {100, 400, 200}, mv::Surface{{0, 0, 0}, mv::kLadder, 0});
  const mv::World world = mv::as_world(w);
  mv::MoveState s = mv::stand_at(spec, world, {0, 0, 0});
  mv::MoveCmd up;
  up.forward = 1;
  for (int i = 0; i < 120; i++) mv::step_move(spec, world, s, up, kDt);
  CHECK(s.on_ladder && s.pos.y > 100);
  const double high = s.pos.y;

  mv::MoveCmd hang;  // nothing held: hangs still, no gravity
  for (int i = 0; i < 30; i++) mv::step_move(spec, world, s, hang, kDt);
  CHECK(s.on_ladder && s.pos.y == high);

  mv::MoveCmd down = up;
  down.pitch = -0.8;
  for (int i = 0; i < 30; i++) mv::step_move(spec, world, s, down, kDt);
  CHECK(s.on_ladder && s.pos.y < high);

  mv::MoveCmd off = hang;
  off.jump = true;
  const mv::MoveEvents ev = mv::step_move(spec, world, s, off, kDt);
  CHECK(ev.has(mv::kLadderOff) && !s.on_ladder);
  CHECK(s.vel.z < 0);  // pushed out along the ladder's normal, back toward -z

  // At its foot, pulling back walks away from it.
  mv::MoveState f = mv::stand_at(spec, world, {0, 0, 100 - spec.radius - 0.5});
  mv::MoveCmd back;
  back.forward = -1;
  for (int i = 0; i < 30; i++) mv::step_move(spec, world, f, back, kDt);
  CHECK(!f.on_ladder && f.grounded && f.pos.z < 100 - spec.radius - 10);
}
