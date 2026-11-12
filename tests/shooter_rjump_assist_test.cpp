// THE ROCKET JUMP HELPER'S COMMAND: the down shot looks 80 degrees down at the player's own yaw,
// the back shot turns half round at his own pitch with the moves turned too (the wish unchanged),
// both hold attack and jump and drop the impulse; a request is taken from its impulse, waits with
// the jump held back, ends after a second or at once when the launcher can never fire, and fires
// on the first command the launcher is ready.
//
// ge:summary Tests: the rocket jump helper's rewritten command and its waiting request
// ge:keywords test rocket-jump rjump rrjump rjump_command rjump_apply
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/rjump_assist.hpp"
#include "shooter/vec.hpp"

namespace sh = ge::shooter;

namespace {

sh::MoveCmd held() {
  sh::MoveCmd c;
  c.msec = 13;
  c.angles = {-10, 30, 0};
  c.forward = 400;
  c.side = -350;
  c.buttons = sh::kButtonJump;
  return c;
}

// The move's wish on the ground: the forward and side moves along the yaw's flat axes.
sh::Vec3f wish(const sh::MoveCmd& c) {
  const sh::ViewAxes a = sh::angle_vectors({0, c.angles.y, 0});
  return {a.forward.x * c.forward + a.right.x * c.side,
          a.forward.y * c.forward + a.right.y * c.side, 0};
}

}  // namespace

TEST(the_impulses_name_the_two_helpers) {
  CHECK(sh::rjump_of(164) == sh::Rjump::down);
  CHECK(sh::rjump_of(156) == sh::Rjump::back);
  CHECK(sh::rjump_of(7) == sh::Rjump::none);
  CHECK(sh::rjump_impulse(sh::Rjump::down) == 164);
  CHECK(sh::rjump_impulse(sh::Rjump::back) == 156);
  CHECK(std::string(sh::rjump_name(sh::Rjump::back)) == "rrjump");
}

TEST(the_down_shot_looks_80_down_at_the_player_s_own_yaw_with_attack_and_jump) {
  sh::MoveCmd c = held();
  c.impulse = 164;
  const sh::MoveCmd out = sh::rjump_command(c, sh::Rjump::down);
  CHECK(out.angles.x == 80.0f && out.angles.y == 30.0f && out.angles.z == 0.0f);
  CHECK(out.forward == 400 && out.side == -350);
  CHECK(out.buttons == (sh::kButtonAttack | sh::kButtonJump));
  CHECK(out.impulse == 0);
  CHECK(out.msec == 13);
}

TEST(the_back_shot_turns_half_round_and_keeps_the_move_s_wish) {
  for (const float yaw : {30.0f, 200.0f, -170.0f, 0.0f, 359.0f}) {
    sh::MoveCmd c = held();
    c.angles.y = yaw;
    const sh::MoveCmd out = sh::rjump_command(c, sh::Rjump::back);
    CHECK(out.angles.x == -10.0f);  // the player's own pitch
    float turn = out.angles.y - yaw;
    if (turn < 0) turn += 360;
    CHECK_MSG(turn == 180.0f, "yaw " + std::to_string(yaw) + " turned " + std::to_string(turn));
    const sh::Vec3f a = wish(c), b = wish(out);
    CHECK_MSG(std::abs(a.x - b.x) < 0.01f && std::abs(a.y - b.y) < 0.01f,
              "the wish moved at yaw " + std::to_string(yaw));
    CHECK((out.buttons & sh::kButtonAttack) != 0 && (out.buttons & sh::kButtonJump) != 0);
  }
}

TEST(a_request_is_taken_from_its_impulse_and_fires_when_the_launcher_is_ready) {
  sh::RjumpSeat seat;
  sh::MoveCmd c = held();
  CHECK(sh::rjump_take(seat, c) == sh::Rjump::none);  // no request
  CHECK(seat.kind == sh::Rjump::none);
  c.impulse = 156;
  CHECK(sh::rjump_take(seat, c) == sh::Rjump::back);
  CHECK(c.impulse == 0 && seat.kind == sh::Rjump::back);
  CHECK(sh::rjump_apply(seat, c, sh::RjumpReady::now));
  CHECK(seat.kind == sh::Rjump::none);
  CHECK((c.buttons & sh::kButtonAttack) != 0);
}

TEST(a_waiting_request_holds_the_jump_back_and_ends_after_a_second) {
  sh::RjumpSeat seat;
  sh::MoveCmd c = held();
  c.impulse = 164;
  sh::rjump_take(seat, c);
  int commands = 0;
  while (seat.kind != sh::Rjump::none && commands < 1000) {
    sh::MoveCmd k = held();
    CHECK(!sh::rjump_apply(seat, k, sh::RjumpReady::soon));
    CHECK((k.buttons & sh::kButtonJump) == 0);  // the jump waits for the shot
    commands++;
  }
  CHECK_MSG(commands == 1000 / 13 + 1, std::to_string(commands) + " commands waited");
  // Waiting, then ready: the shot comes with the jump.
  c.impulse = 164;
  sh::rjump_take(seat, c);
  sh::MoveCmd k = held();
  CHECK(!sh::rjump_apply(seat, k, sh::RjumpReady::soon));
  k = held();
  CHECK(sh::rjump_apply(seat, k, sh::RjumpReady::now));
  CHECK((k.buttons & sh::kButtonJump) != 0 && k.angles.x == 80.0f);
}

TEST(a_request_the_launcher_can_never_fire_ends_at_once_and_leaves_the_command) {
  sh::RjumpSeat seat;
  sh::MoveCmd c = held();
  c.impulse = 164;
  sh::rjump_take(seat, c);
  sh::MoveCmd k = held();
  CHECK(!sh::rjump_apply(seat, k, sh::RjumpReady::never));
  CHECK(seat.kind == sh::Rjump::none);
  CHECK(k.buttons == sh::kButtonJump && k.angles.x == -10.0f);
  // With nothing waiting, a command passes untouched.
  sh::MoveCmd j = held();
  CHECK(!sh::rjump_apply(seat, j, sh::RjumpReady::now));
  CHECK(j.angles.x == -10.0f && j.buttons == sh::kButtonJump);
}
