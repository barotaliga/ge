// plugins/shooter/aim_turn: the aim help's turn of the view. A bend onto a player turns the view
// toward him with inertia while the attack is held, the mouse still turning the view; letting go
// of the attack brings the view back to where the player aimed; a target far off his aim is let
// go.
//
// ge:summary Test: the aim help's view turn toward a bent-onto player and back
// ge:keywords test aim-turn spring inertia view
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "shooter/aim_turn.hpp"

using namespace ge::shooter;

namespace {

constexpr float kDt = 1.0f / 60;

void run(AimTurn& a, Vec3f& view, Vec3f eye, const Vec3f* at, bool attack, int frames) {
  for (int i = 0; i < frames; i++) aim_turn_step(a, view, eye, at, attack, kDt);
}

}  // namespace

TEST(a_bend_turns_the_view_onto_the_target_and_back_when_the_attack_ends) {
  AimTurn a;
  Vec3f view{0, 0, 0};  // looking along +x
  const Vec3f eye{0, 0, 0};
  const Vec3f target{500, 30, 0};  // 3.4 degrees to the left
  aim_turn_bent(a, 2);
  run(a, view, eye, &target, true, 3);
  CHECK(view.y > 0.3f && view.y < 3.3f);  // on its way, not there at once
  run(a, view, eye, &target, true, 60);
  CHECK(std::abs(view.y - 3.43f) < 0.05f);
  run(a, view, eye, &target, false, 90);  // the attack let go: back to his own aim
  CHECK(a.target == -1 && std::abs(view.y) < 0.01f && a.yaw == 0);
}

TEST(the_mouse_keeps_turning_the_view_while_it_is_held_and_after) {
  AimTurn a;
  Vec3f view{0, 0, 0};
  const Vec3f eye{0, 0, 0};
  const Vec3f target{500, 30, 0};
  aim_turn_bent(a, 2);
  run(a, view, eye, &target, true, 60);
  view.y += 2;  // the mouse turns two degrees to the left
  run(a, view, eye, &target, true, 60);
  CHECK(std::abs(view.y - 3.43f) < 0.05f);  // still on him: the turn took up the mouse's turn
  run(a, view, eye, &target, false, 120);
  CHECK(std::abs(view.y - 2) < 0.01f);  // back where the mouse left his own aim
}

TEST(a_target_far_off_the_players_aim_or_gone_is_let_go) {
  AimTurn a;
  Vec3f view{0, 0, 0};
  const Vec3f eye{0, 0, 0};
  const Vec3f far_off{500, 300, 0};  // 300 units off his aim at 500
  aim_turn_bent(a, 2);
  run(a, view, eye, &far_off, true, 1);
  CHECK(a.target == -1);
  aim_turn_bent(a, 3);
  run(a, view, eye, nullptr, true, 1);
  CHECK(a.target == -1 && std::abs(view.y) < 0.01f);
}
