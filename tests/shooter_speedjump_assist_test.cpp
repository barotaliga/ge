// THE EASY JUMP ON A STOCK SERVER: forward and jump held, sent through `assisted_command` to a
// server running the classic move alone (pm_speedjump 0), gain about what the server's own easy
// jump gains, less than the best strafe; the commands it leaves alone go out as held.
//
// ge:summary Tests: the client's easy-jump commands on a classic-only server gain like the server's
// ge:keywords test speedjump assist assisted_command easy-jump stock-server
// ge:skill shooter testing
#include "check.hpp"
#include "core/libm/libm.hpp"
#include "shooter/pmove.hpp"
#include "shooter/recording.hpp"
#include "shooter/speedjump_assist.hpp"

namespace sh = ge::shooter;

namespace {

constexpr int kRate = 77;

struct Field {
  sh::SceneWorld sw;
  Field() {
    sh::RecordedScene scene;
    sh::Brush b;
    const float far = 32768;
    b.planes = {{{1, 0, 0}, far},  {{-1, 0, 0}, far}, {{0, 1, 0}, far},
                {{0, -1, 0}, far}, {{0, 0, 1}, 0},    {{0, 0, -1}, 64}};
    scene.brushes.push_back(b);
    sh::build_scene_world(scene, {}, sw);
  }
};

float plan(sh::Vec3f v) { return sh::root_of(v.x * v.x + v.y * v.y); }

// A second's run east, then `seconds` of forward and jump held at view yaw `yaw`; `assist` sends
// them through the assist to a classic server, else to a server running the easy jump itself.
sh::MoveState run(const Field& f, bool assist, int seconds, float yaw = 0, int* jumps = nullptr) {
  sh::MoveSettings classic, easy;
  easy.speedjump = 0.5f;
  const sh::MoveSettings& set = assist ? classic : easy;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  for (int i = 0; i < kRate; i++) sh::player_move(s, cmd, set, f.sw.world);
  cmd.buttons = sh::kButtonJump;
  cmd.angles = {0, yaw, 0};
  for (int i = 0; i < seconds * kRate; i++) {
    const bool down = s.ground != -1;
    const sh::MoveCmd sent = assist ? sh::assisted_command(cmd, s, 0.5f) : cmd;
    sh::player_move(s, sent, set, f.sw.world);
    if (jumps && down && s.ground == -1) (*jumps)++;
  }
  return s;
}

}  // namespace

TEST(the_assist_gains_like_the_servers_easy_jump) {
  Field f;
  int jumps = 0;
  const sh::MoveState aided = run(f, true, 8, 0, &jumps);
  const sh::MoveState served = run(f, false, 8);
  CHECK(jumps >= 10);  // the jump let go in the air, pressed on landing
  CHECK_MSG(plan(aided.velocity) > 520, "8 s of the assist should pass 520 units/s");
  const float ratio = plan(aided.velocity) / plan(served.velocity);
  CHECK_MSG(ratio > 0.9f && ratio < 1.1f, "the assist should gain within 10% of the server's");
  CHECK(plan(aided.velocity) < 780);  // the best strafe reaches about 810
}

TEST(the_assist_keeps_the_way_on_the_view) {
  Field f;
  const sh::MoveState s = run(f, true, 3, 30);
  const double way = ge::core::libm::atan2(s.velocity.y, s.velocity.x) * 180 / 3.14159265358979;
  CHECK_MSG(way > 24 && way < 36, "the way should follow the view to about 30 degrees");
}

TEST(the_assist_leaves_other_commands_as_held) {
  sh::MoveState air;
  air.ground = -1;
  air.velocity = {400, 0, 0};
  sh::MoveCmd fire;
  fire.forward = 400;
  fire.buttons = sh::kButtonJump | sh::kButtonAttack;
  fire.angles = {10, 5, 0};
  const sh::MoveCmd a = sh::assisted_command(fire, air, 0.5f);
  CHECK(a.angles == fire.angles && a.forward == 400 && a.side == 0 && a.buttons == fire.buttons);
  sh::MoveCmd strafe;
  strafe.forward = 400;
  strafe.side = -400;
  const sh::MoveCmd b = sh::assisted_command(strafe, air, 0.5f);
  CHECK(b.side == -400 && b.forward == 400 && b.angles == strafe.angles);
  // Standing, the held jump goes out pressed.
  sh::MoveState stand;
  stand.ground = 0;
  sh::MoveCmd jump;
  jump.forward = 400;
  jump.buttons = sh::kButtonJump;
  CHECK(sh::assisted_command(jump, stand, 0.5f).buttons == sh::kButtonJump);
  // Flying, it is let go, and the forward key becomes the side key.
  const sh::MoveCmd c = sh::assisted_command(jump, air, 0.5f);
  CHECK(c.buttons == 0 && c.forward == 0 && c.side == 400);
  // Share 0: nothing.
  CHECK(sh::assisted_command(jump, air, 0).buttons == sh::kButtonJump);
}
