// ge:summary The bot's aim: the skill profiles, the angles to a point, the axes turned under a view
// ge:keywords aim aim_profile aim_angles turn_axes skill
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooter/aim.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::shooter {

const char* aim_skill_name(AimSkill s) {
  switch (s) {
    case AimSkill::perfect:
      return "perfect";
    case AimSkill::novice:
      return "novice";
    case AimSkill::average:
      return "average";
    case AimSkill::expert:
      return "expert";
  }
  return "?";
}

AimProfile aim_profile(AimSkill s) {
  AimProfile p;
  p.hands.resize(2);
  for (ai::AxisHands& a : p.hands) a.pass = true;
  if (s == AimSkill::perfect) return p;
  // Each skill: the reaction, the wander, the hand's settling and overshoot, the fastest turn.
  struct Row {
    double delay_s, wander, wander_hz, settle_s, damping, slew;
    float lead_s;
  };
  const Row rows[3] = {
      {0.30, 3.0, 0.5, 0.15, 0.7, 360, 0.0f},   // novice
      {0.22, 1.5, 0.6, 0.10, 0.8, 600, 0.1f},   // average
      {0.17, 0.6, 0.7, 0.07, 0.9, 1000, 0.2f},  // expert
  };
  const Row& r = rows[static_cast<int>(s) - 1];
  p.hands[0] = {false,     r.delay_s, r.wander, r.wander_hz, r.settle_s,
                r.damping, r.slew,    -1e9,     1e9,         0};
  p.hands[1] = {false,      r.delay_s, r.wander * 0.5, r.wander_hz,
                r.settle_s, r.damping, r.slew * 0.5,   -70,
                80,         0};
  p.lead_s = r.lead_s;
  return p;
}

void aim_angles(Vec3f eye, Vec3f at, float& yaw, float& pitch) {
  const double dx = at.x - eye.x, dy = at.y - eye.y, dz = at.z - eye.z;
  yaw = static_cast<float>(core::libm::atan2(dy, dx) * (180.0 / core::kPi));
  const double flat = std::sqrt(dx * dx + dy * dy);
  pitch = static_cast<float>(-core::libm::atan2(dz, flat) * (180.0 / core::kPi));
}

void turn_axes(int16_t& forward, int16_t& side, float from_yaw, float to_yaw) {
  // The wish f * fwd(from) + s * right(from), read on fwd(to) and right(to): with d = from - to,
  // forward' = f cos d + s sin d, side' = s cos d - f sin d.
  const Vec3f cs = angle_vectors({0, from_yaw - to_yaw, 0}).forward;  // cos d, sin d
  const float f = forward, s = side;
  // Rounded first: a right angle's cosine is a few units of float off zero, and the grid rounds
  // down, so -1e-5 would press -4.
  forward = quantize_axis(std::round(f * cs.x + s * cs.y));
  side = quantize_axis(std::round(s * cs.x - f * cs.y));
}

}  // namespace ge::shooter
