// The pilot's air strafe (plugins/shooterbots/pilot.hpp `air_strafe`): a flight off the line to its
// target turns on the side key with its wish never past 90 degrees off the velocity, so the turn
// takes nothing off the speed. Led into the turn instead (the view 45 degrees ahead of the velocity
// with the side key alone), the wish lay 135 off and braked: on e1m4 a jump over the slime fell
// from 302 units/s to a stop in a tenth of a second and dropped into the pit.
//
// ge:summary Tests for the pilot's air strafe: the side key's wish within 90 of the velocity, no brake
// ge:keywords test pilot air air_strafe strafe side-key wish brake jump flight slime e1m4
// ge:skill shooterbots testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/predict.hpp"
#include "shooter_loop_bench.hpp"
#include "shooterbots/pilot.hpp"

namespace {

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace lm = ge::core::libm;
using ge::test::loop::box;

constexpr double kDeg = 180 / ge::core::kPi;

float heading_of(sh::Vec3f v) { return static_cast<float>(lm::atan2(v.y, v.x) * kDeg); }
float speed_of(sh::Vec3f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
float wrap180(float a) {
  while (a > 180) a -= 360;
  while (a <= -180) a += 360;
  return a;
}

// A floor far under the flight: open air for the commands flown.
struct OpenAir {
  sh::ModelData data = sh::build_brush_model({box({-4096, -4096, -64}, {4096, 4096, 0})});
  sh::BrushModel model = data.model();
  sh::MoveWorld world;
  OpenAir() { sh::client_world(model, nullptr, 0, nullptr, 0, world); }
};

// The side key's wish under the view `st`: 90 degrees off it toward the key.
float wish_heading(const sb::AirStrafe& st) { return wrap180(st.yaw - st.side * 90); }

// A flight at 302 units/s heading 90 toward an aim `aim_off` degrees off, its keys from `keys`
// each command; the speed across and the heading after `n` commands.
template <typename Keys>
void fly(const OpenAir& air, float aim_off, int n, Keys keys, float& speed, float& heading) {
  sh::MoveSettings set;
  sh::MoveState s;
  s.origin = {0, 0, 2000};
  s.velocity = {0, 302, 180};
  const float aim = 90 + aim_off;
  for (int i = 0; i < n; i++) {
    const float vh = heading_of(s.velocity);
    const sb::AirStrafe st = keys(vh, wrap180(aim - vh), speed_of(s.velocity));
    sh::MoveCmd c;
    c.msec = 14;
    c.angles = {0, st.yaw, 0};
    c.side = static_cast<int16_t>(st.side * 400);
    sh::run_command(s, c, set, air.world);
  }
  speed = speed_of(s.velocity);
  heading = heading_of(s.velocity);
}

}  // namespace

TEST(the_side_keys_wish_lies_within_90_of_the_velocity_toward_the_turn) {
  for (const float speed : {120.f, 302.f, 450.f, 700.f}) {
    for (const float err : {-120.f, -40.f, -12.f, -5.f, -1.f, 1.f, 5.f, 12.f, 40.f, 120.f}) {
      for (const float cap : {30.f, 0.f}) {
        const sb::AirStrafe st = sb::air_strafe(20, err, speed, cap);
        const float off = wrap180(wish_heading(st) - 20);
        const std::string at = "speed " + std::to_string(speed) + " err " + std::to_string(err) +
                               " cap " + std::to_string(cap);
        CHECK_MSG((off > 0) == (err > 0), at + ": the wish turns away from the aim");
        CHECK_MSG(off <= 90.01f && off >= -90.01f,
                  at + ": the wish lies " + std::to_string(off) +
                      " off the velocity, past 90: it brakes (air_strafe's across)");
      }
    }
  }
}

TEST(a_flight_turned_onto_its_target_keeps_its_speed) {
  const OpenAir air;
  float speed = 0, heading = 0;
  fly(
      air, -25, 24, [](float vh, float err, float v) { return sb::air_strafe(vh, err, v, 30); },
      speed, heading);
  CHECK_MSG(speed >= 302 * 0.98f, "the strafe took the flight from 302 to " +
                                      std::to_string(speed) +
                                      " units/s: its wish lay past 90 off the velocity");
  CHECK_MSG(heading < 90 - 10, "the flight turned only to " + std::to_string(heading) +
                                   " toward an aim at 65: the side key's wish lay too near it");
  // The view led 45 degrees into the turn with the side key alone (as before): it brakes.
  float led_speed = 0, led_heading = 0;
  fly(
      air, -25, 24,
      [](float vh, float err, float) {
        sb::AirStrafe st;
        st.side = err > 0 ? -1.f : 1.f;
        st.yaw = vh - st.side * 45;
        return st;
      },
      led_speed, led_heading);
  CHECK_MSG(led_speed < speed * 0.5f, "the led view kept " + std::to_string(led_speed) +
                                          " units/s: the test no longer "
                                          "shows the brake it guards against");
}
