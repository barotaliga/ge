// Tests: a mover's motion frame by frame (plugins/shooter/mover_motion.hpp) on a bare struct with
// the fields it names: the plain move ends on its place to the bit with whole frames at the speed
// and one last frame of the rest; the accelerated move speeds up, holds and slows, never faster
// than its speed, and ends on its place; a short accelerated move goes in one push; the turn ends
// on its angles; a move of nothing arrives at once.
//
// ge:summary Tests: the mover kit's plain and accelerated moves and the turn, each ending exactly
// ge:keywords test mover mover-kit begin_move begin_turn step_motion accelerate door plat
// ge:skill shooter testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "shooter/mover_motion.hpp"

namespace sh = ge::shooter;

namespace {

struct Mover {
  sh::Vec3f origin, angles, dest, adest, dir, velocity, avelocity;
  float speed = 0, accel = 0, decel = 0;
  int frames = 0;
  float rest = 0, now_speed = 0, next_speed = 0, top = 0, decel_span = 0;
  bool last = false, accelerated = false;
};

constexpr sh::MoverFrame kFrame{};

// Runs `m` to its end as a server does (the push of each frame's velocity, then the step), the
// frames it took; the fastest frame's speed in `fastest`.
int run(Mover& m, bool turns, float* fastest = nullptr) {
  for (int n = 1; n < 1000; n++) {
    m.origin = m.origin + m.velocity * kFrame.step;
    m.angles = m.angles + m.avelocity * kFrame.step;
    if (fastest != nullptr) *fastest = std::max(*fastest, sh::length(m.velocity));
    if (sh::step_motion(m, turns, kFrame) == sh::MotionStep::arrived) return n;
  }
  return -1;
}

}  // namespace

TEST(the_plain_move_ends_on_its_place) {
  Mover m;
  m.speed = m.accel = m.decel = 100;
  m.origin = {0, 0, 0};
  sh::begin_move(m, {0, 0, 125}, kFrame);
  CHECK(!m.accelerated && m.frames == 12);
  CHECK(run(m, false) == 13);  // twelve whole frames, one of the rest, then arrived
  CHECK(sh::length(m.origin - sh::Vec3f{0, 0, 125}) < 1e-3f);
  CHECK(m.velocity.z == 0);
}

TEST(the_accelerated_move_speeds_up_holds_slows_and_ends_on_its_place) {
  Mover m;
  m.speed = 20;  // per frame: 200 units a second
  m.accel = m.decel = 4;
  sh::begin_move(m, {300, 0, 0}, kFrame);
  CHECK(m.accelerated);
  m.velocity = {};
  float fastest = 0;
  // The first frame plans the move (the server steps it before the first push).
  CHECK(sh::step_motion(m, false, kFrame) == sh::MotionStep::moving);
  const int frames = run(m, false, &fastest);
  CHECK(frames > 15 && frames < 40);
  CHECK(fastest <= 200 + 1e-3f);
  CHECK_MSG(sh::length(m.origin - sh::Vec3f{300, 0, 0}) < 1e-2f, std::to_string(m.origin.x));
}

TEST(a_short_accelerated_move_goes_in_one_push) {
  Mover m;
  m.speed = 20;
  m.accel = m.decel = 4;
  sh::begin_move(m, {0, 3, 0}, kFrame);
  CHECK(sh::step_motion(m, false, kFrame) == sh::MotionStep::moving && m.last);
  CHECK(run(m, false) == 1);
  CHECK(sh::length(m.origin - sh::Vec3f{0, 3, 0}) < 1e-4f);
}

TEST(the_turn_ends_on_its_angles_and_a_move_of_nothing_arrives_at_once) {
  Mover m;
  m.speed = 90;
  sh::begin_turn(m, {0, 90, 0}, kFrame);
  CHECK(m.frames == 10);
  run(m, true);
  CHECK(m.angles.y > 89.99f && m.angles.y < 90.01f);
  Mover still;
  still.speed = still.accel = still.decel = 50;
  sh::begin_move(still, still.origin, kFrame);
  CHECK(still.velocity.x == 0 && still.velocity.y == 0 && still.velocity.z == 0);
  CHECK(run(still, false) == 1);
}
