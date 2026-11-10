// The circle jump (shooter/circle_jump.hpp) flown through the move on a flat floor: a run held
// along its velocity stays at 320; the curve's keys and view lift it past 420 before its jump, the
// side turning toward the aim, as people's standing jumps do.
//
// ge:summary Test: the circle jump flown through the move lifts a run past 420 before its jump
// ge:keywords circle-jump ground-strafe prestrafe circle_step circle_angle takeoff-speed
// ge:skill shooter bots
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/circle_jump.hpp"
#include "shooter/pmove.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(int contents, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

struct Floor {
  sh::HullData point, clip;
  sh::MoveWorld world;
  Floor() {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-8192, -8192, -64}, {8192, 8192, 0})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.count = 1;
  }
};

constexpr int kMsec = 13;
constexpr float kDt = kMsec * 0.001f;
constexpr double kDeg = 180 / ge::core::kPi;

float across(sh::Vec3f v) { return static_cast<float>(ge::core::hypot(v.x, v.y)); }
float heading(sh::Vec3f v) { return static_cast<float>(ge::core::libm::atan2(v.y, v.x) * kDeg); }

// A run at 320 along +x on the floor, the aim `aim` degrees off it (+ left), then the curve; the
// takeoff's speed and the side the curve took (0: no jump within 1 s).
struct Takeoff {
  float speed = 0, after = 0, time_s = 0;
  int8_t side = 0;
};

Takeoff circle(float aim, const sh::CircleSettings& set) {
  static const Floor f;
  sh::MoveSettings move;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  s.velocity = {320, 0, 0};
  s.maxspeed = move.maxspeed;
  sh::MoveCmd c;
  c.msec = kMsec;
  c.angles = {0, 0, 0};
  sh::player_move(s, c, move, f.world);  // settled on the floor
  s.velocity = {320, 0, 0};
  Takeoff out;
  int8_t side = 0;
  for (int i = 0; i < 80; i++) {
    const float h = heading(s.velocity);
    float err = aim - h;
    while (err > 180) err -= 360;
    while (err < -180) err += 360;
    const sh::CircleStep step = sh::circle_step(h, across(s.velocity), err,
                                                static_cast<float>(i) * kDt, kDt, side, move, set);
    sh::MoveCmd m;
    m.msec = kMsec;
    if (step.jump) {
      out.speed = across(s.velocity);
      out.time_s = static_cast<float>(i) * kDt;
      out.side = side;
      m.angles = {0, h, 0};
      m.buttons = sh::kButtonJump;
      sh::player_move(s, m, move, f.world);
      out.after = across(s.velocity);
      return out;
    }
    side = step.side;
    m.angles = {0, step.yaw, 0};
    m.forward = static_cast<int16_t>(move.maxspeed);
    m.side = static_cast<int16_t>(step.side * move.maxspeed);
    sh::player_move(s, m, move, f.world);
  }
  return out;
}

}  // namespace

TEST(a_run_along_its_velocity_stays_at_a_runs_cap) {
  static const Floor f;
  sh::MoveSettings move;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  s.velocity = {320, 0, 0};
  s.maxspeed = move.maxspeed;
  for (int i = 0; i < 40; i++) {
    sh::MoveCmd m;
    m.msec = kMsec;
    m.forward = static_cast<int16_t>(move.maxspeed);
    sh::player_move(s, m, move, f.world);
  }
  CHECK(across(s.velocity) <= 321);
}

TEST(the_best_angle_is_none_under_the_runs_cap_and_about_fifty_degrees_at_speed) {
  sh::MoveSettings move;
  CHECK(sh::circle_angle(250, kDt, move) == 0);
  const float a = sh::circle_angle(450, kDt, move);
  CHECK(a > 45 && a < 55);
}

TEST(the_curve_lifts_a_run_past_420_before_its_jump_turning_toward_the_aim) {
  sh::CircleSettings set;
  const Takeoff left = circle(70, set);
  CHECK(left.speed >= 420);
  CHECK(left.time_s <= set.most_s + kDt);
  CHECK(left.side == -1);    // the aim on the left: the left key
  CHECK(left.after >= 415);  // the jump on the ground's last command keeps the speed
  const Takeoff right = circle(-70, set);
  CHECK(right.speed >= 420);
  CHECK(right.side == 1);
}

TEST(the_curve_jumps_once_it_has_turned_past_the_aim) {
  sh::CircleSettings set;
  set.target = 1000;  // never fast enough: the turn or the time ends it
  set.most_s = 2;
  const Takeoff t = circle(30, set);  // a small turn: past it in well under a second
  CHECK(t.time_s > 0 && t.time_s < 0.5f);
}

TEST(a_straight_way_is_jumped_at_once_without_a_curve) {
  const Takeoff t = circle(10, sh::CircleSettings{});
  CHECK(t.time_s == 0);
  CHECK(t.speed <= 321);
}
