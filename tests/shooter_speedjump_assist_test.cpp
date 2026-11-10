// THE EASY JUMP: forward and jump held, sent through `assisted_command` to the classic move, hop on
// and gain at the share asked (less than the best strafe at under 1); the way weaves across the
// aim, follows the mouse, ignores the pitch; the screen shows the asked share of the swing; the
// commands it leaves alone go out as held, a shot along the shown view.
//
// ge:summary Tests: the easy jump's strafing commands gain, weave, steer and show their swing
// ge:keywords test speedjump assist assisted_command easy-jump swing cl_speedjump_view
// ge:skill shooter testing
#include <algorithm>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "shooter/pmove.hpp"
#include "shooter/recording.hpp"
#include "shooter/speedjump_assist.hpp"

namespace sh = ge::shooter;

namespace {

constexpr int kRate = 77;
constexpr double kDeg = 180 / 3.14159265358979323846;

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
double heading(sh::Vec3f v) { return ge::core::libm::atan2(v.y, v.x) * kDeg; }
double wrap(double a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

struct Run {
  sh::MoveState s;
  sh::SpeedJumpAid aid;
  int jumps = 0;
  std::vector<double> off;   // the way off the aim, each command in the air after 2 s
  std::vector<float> shown;  // the shown swing, each command
  std::vector<float> swing;  // the sent swing, each command
};

// A second's run east, then `seconds` of forward and jump held at aim `yaw` and pitch `pitch`.
Run run(const Field& f, float share, int seconds, float yaw = 0, float view = 0.5f,
        float pitch = 0) {
  const sh::MoveSettings set;
  Run r;
  r.s.origin = {0, 0, 24};
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  for (int i = 0; i < kRate; i++) sh::player_move(r.s, cmd, set, f.sw.world);
  cmd.buttons = sh::kButtonJump;
  cmd.angles = {pitch, yaw, 0};
  for (int i = 0; i < seconds * kRate; i++) {
    const bool down = r.s.ground != -1;
    const sh::MoveCmd sent = sh::assisted_command(cmd, r.s, share, view, r.aid);
    sh::player_move(r.s, sent, set, f.sw.world);
    if (down && r.s.ground == -1) r.jumps++;
    if (i >= 2 * kRate && r.s.ground == -1) r.off.push_back(wrap(heading(r.s.velocity) - yaw));
    r.shown.push_back(r.aid.shown);
    r.swing.push_back(r.aid.swing);
  }
  return r;
}

// The best strafe: the side key alone, swapped each hop, the view on the way.
float best_strafe(const Field& f, int seconds) {
  const sh::MoveSettings set;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  for (int i = 0; i < kRate; i++) sh::player_move(s, cmd, set, f.sw.world);
  int side = 400;
  for (int i = 0; i < seconds * kRate; i++) {
    const bool down = s.ground != -1;
    cmd = {};
    cmd.msec = 13;
    if (down) side = -side;
    cmd.side = static_cast<int16_t>(side);
    cmd.buttons = down ? sh::kButtonJump : 0;
    cmd.angles = {0, static_cast<float>(heading(s.velocity)), 0};
    sh::player_move(s, cmd, set, f.sw.world);
  }
  return plan(s.velocity);
}

}  // namespace

TEST(the_easy_jump_hops_on_and_gains_by_its_share) {
  Field f;
  const Run half = run(f, 0.5f, 8), most = run(f, 0.8f, 8), all = run(f, 1, 8);
  CHECK(most.jumps >= 10);  // the jump let go in the air, pressed on landing
  CHECK_MSG(plan(half.s.velocity) > 500, "8 s at share 0.5 should pass 500 units/s");
  CHECK_MSG(plan(most.s.velocity) > plan(half.s.velocity) + 50, "a larger share gains more");
  CHECK_MSG(plan(all.s.velocity) > plan(most.s.velocity), "share 1 gains the most");
  CHECK_MSG(plan(most.s.velocity) < best_strafe(f, 8), "under 1 the best strafe gains more");
}

TEST(the_way_weaves_across_the_aim) {
  Field f;
  const Run r = run(f, 0.8f, 8);
  const double lo = *std::min_element(r.off.begin(), r.off.end());
  const double hi = *std::max_element(r.off.begin(), r.off.end());
  CHECK_MSG(lo < -10 && hi > 10, "the way should swing well to both sides of the aim");
  CHECK_MSG(lo > -sh::kSpeedJumpSwing - 6 && hi < sh::kSpeedJumpSwing + 6,
            "the swing should turn back near kSpeedJumpSwing");
  double sum = 0;
  for (const double o : r.off) sum += o;
  const double mean = sum / static_cast<double>(r.off.size());
  CHECK_MSG(mean > -6 && mean < 6, "on average the hops go where he aims");
}

TEST(the_mouse_steers_and_the_pitch_does_not) {
  Field f;
  const Run left = run(f, 0.8f, 4, 90);  // the aim turned a quarter left of the run-up
  CHECK_MSG(wrap(heading(left.s.velocity) - 90) > -sh::kSpeedJumpSwing - 6 &&
                wrap(heading(left.s.velocity) - 90) < sh::kSpeedJumpSwing + 6,
            "the hops should have turned to the aim");
  CHECK(plan(left.s.velocity) > 400);  // turning, it still gains
  const Run level = run(f, 0.8f, 3), up = run(f, 0.8f, 3, 0, 0.5f, -70);
  CHECK(level.s.velocity.x == up.s.velocity.x && level.s.velocity.y == up.s.velocity.y);
}

TEST(the_view_shows_its_share_of_the_swing) {
  Field f;
  const Run still = run(f, 0.8f, 4, 0, 0), all = run(f, 0.8f, 4, 0, 1), half = run(f, 0.8f, 4);
  float most_still = 0, most_all = 0, most_half = 0, most_swing = 0;
  for (size_t i = 0; i < all.shown.size(); i++) {
    most_still = std::max(most_still, std::abs(still.shown[i]));
    most_all = std::max(most_all, std::abs(all.shown[i]));
    most_half = std::max(most_half, std::abs(half.shown[i]));
    most_swing = std::max(most_swing, std::abs(all.swing[i]));
  }
  CHECK(most_still == 0);
  CHECK_MSG(most_all > 10 && most_all <= most_swing, "view 1 shows the swing");
  CHECK_MSG(most_half > 0.4f * most_all && most_half < 0.6f * most_all, "view 0.5 shows half");
}

TEST(the_easy_jump_leaves_other_commands_as_held) {
  sh::MoveState air;
  air.ground = -1;
  air.velocity = {400, 0, 0};
  sh::SpeedJumpAid aid;
  sh::MoveCmd strafe;
  strafe.forward = 400;
  strafe.side = -400;
  const sh::MoveCmd b = sh::assisted_command(strafe, air, 0.8f, 1, aid);
  CHECK(b.side == -400 && b.forward == 400 && b.angles == strafe.angles);
  // Standing, the held jump goes out pressed.
  sh::MoveState stand;
  stand.ground = 0;
  sh::MoveCmd jump;
  jump.forward = 400;
  jump.buttons = sh::kButtonJump;
  CHECK(sh::assisted_command(jump, stand, 0.8f, 1, aid).buttons == sh::kButtonJump);
  // Flying, it is let go, and the forward key becomes the side key toward the turn.
  const sh::MoveCmd c = sh::assisted_command(jump, air, 0.8f, 1, aid);
  CHECK(c.buttons == 0 && c.forward == 0 && (c.side == 400 || c.side == -400));
  // The sent yaw stays near the way, as a strafer looks.
  CHECK(std::abs(wrap(c.angles.y)) < 45);
  // A shot carries the view the screen shows.
  sh::MoveCmd fire = jump;
  fire.buttons = sh::kButtonJump | sh::kButtonAttack;
  fire.angles = {10, 5, 0};
  const float shown = aid.shown;
  const sh::MoveCmd a = sh::assisted_command(fire, air, 0.8f, 1, aid);
  CHECK(a.forward == 400 && a.side == 0 && a.buttons == fire.buttons && a.angles.x == 10);
  CHECK(std::abs(a.angles.y - (5 + shown)) < 0.001f);
  // Share 0: nothing.
  sh::SpeedJumpAid none;
  CHECK(sh::assisted_command(jump, air, 0, 1, none).buttons == sh::kButtonJump);
}
