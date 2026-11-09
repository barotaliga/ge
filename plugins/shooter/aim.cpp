// ge:summary The bot's aim: the skill profiles, the angles to a point, the axes turned under a view
// ge:keywords aim aim_profile aim_angles turn_axes skill
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooter/aim.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/traits.hpp"

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

namespace {

// The mouse hand of an aim trait `aim` and a reaction trait `reaction` (1 to 10; 2, 5 and 8 the
// novice, the average and the expert): a 180-degree flip in about 0.5, 0.32 and 0.22 s, landing
// within about 45, 15 and 5 degrees; the expert on a lower sensitivity and a wider reach.
void mouse_of(AimProfile& p, uint8_t aim, uint8_t reaction) {
  const auto at = [](uint8_t v, double a, double b, double c, double d) {
    return scale(v, {a, b, c, d});
  };
  ai::MouseHand& h = p.hand;
  p.mouse = true;
  h.cm_per_360 = at(aim, 34, 30, 26, 24);
  h.reach_cm = at(aim, 9, 11, 13, 14);
  h.lift_s = at(aim, 0.24, 0.17, 0.13, 0.11);
  h.fitts_a = at(aim, 0.10, 0.08, 0.06, 0.05);
  h.fitts_b = at(aim, 0.07, 0.04, 0.027, 0.022);
  h.gain = at(aim, 0.9, 0.97, 0.99, 0.995);
  h.gain_sd = at(aim, 0.17, 0.056, 0.019, 0.012);
  h.dir_sd_deg = at(aim, 9, 3.2, 1.2, 0.8);
  h.setdown_cm = at(aim, 0.15, 0.1, 0.06, 0.05);
  h.look_s = at(reaction, 0.17, 0.13, 0.10, 0.09);
  ai::FingerSpec& f = p.finger;
  f.gap_calm_s = at(reaction, 0.24, 0.2, 0.17, 0.16);
  f.gap_stress_s = at(reaction, 0.13, 0.11, 0.1, 0.095);
  f.sync = at(aim, 0.25, 0.5, 0.8, 0.9);
}

}  // namespace

AimProfile aim_profile(AimSkill s) {
  AimProfile p;
  p.hands.resize(2);
  for (ai::AxisHands& a : p.hands) a.pass = true;
  if (s == AimSkill::perfect) return p;
  // Each skill: the reaction, the wander, the hand's settling and overshoot, the fastest turn.
  struct Row {
    double delay_s, wander, wander_hz, settle_s, damping, slew;
    float lead_s, react_lo, react_med, react_hi;
  };
  // The delay is the hands' tracking lag; a new sighting waits the reaction drawn round
  // `react_med` (a top player about 160 ms, an average one 230, a novice 300).
  const Row rows[3] = {
      {0.20, 3.0, 0.5, 0.15, 0.7, 360, 0.0f, 0.24f, 0.30f, 0.42f},    // novice
      {0.15, 1.5, 0.6, 0.10, 0.8, 600, 0.1f, 0.18f, 0.23f, 0.30f},    // average
      {0.12, 0.6, 0.7, 0.07, 0.9, 1000, 0.14f, 0.14f, 0.16f, 0.22f},  // expert
  };
  const Row& r = rows[static_cast<int>(s) - 1];
  p.hands[0] = {false,     r.delay_s, r.wander, r.wander_hz, r.settle_s,
                r.damping, r.slew,    -1e9,     1e9,         0};
  p.hands[1] = {false,      r.delay_s, r.wander * 0.5, r.wander_hz,
                r.settle_s, r.damping, r.slew * 0.5,   -70,
                80,         0};
  p.lead_s = r.lead_s;
  p.react_lo = r.react_lo;
  p.react_med = r.react_med;
  p.react_hi = r.react_hi;
  const uint8_t trait = static_cast<uint8_t>(s == AimSkill::novice    ? 2
                                             : s == AimSkill::average ? 5
                                                                      : 8);
  mouse_of(p, trait, trait);
  return p;
}

AimProfile aim_profile(uint8_t aim, uint8_t reaction) {
  // Each value at the traits 2, 5 and 8 is the novice's, the average's and the expert's row
  // above; at 10 the best a person does (traits.hpp's `scale` draws straight lines between).
  const auto at = [](uint8_t v, double a, double b, double c, double d) {
    return scale(v, {a, b, c, d});
  };
  const double delay_s = at(reaction, 0.20, 0.15, 0.12, 0.10);
  const double wander = at(aim, 3.0, 1.5, 0.6, 0.3);
  const double wander_hz = at(aim, 0.5, 0.6, 0.7, 0.8);
  const double settle_s = at(aim, 0.15, 0.10, 0.07, 0.05);
  const double damping = at(aim, 0.7, 0.8, 0.9, 0.95);
  const double slew = at(aim, 360, 600, 1000, 1400);
  AimProfile p;
  p.hands.resize(2);
  p.hands[0] = {false, delay_s, wander, wander_hz, settle_s, damping, slew, -1e9, 1e9, 0};
  p.hands[1] = {false, delay_s, wander * 0.5, wander_hz, settle_s, damping, slew * 0.5, -70, 80, 0};
  p.lead_s = static_cast<float>(at(aim, 0.0, 0.1, 0.14, 0.17));
  p.react_lo = static_cast<float>(at(reaction, 0.24f, 0.18f, 0.14f, 0.12f));
  p.react_med = static_cast<float>(at(reaction, 0.30f, 0.23f, 0.16f, 0.14f));
  p.react_hi = static_cast<float>(at(reaction, 0.42f, 0.30f, 0.22f, 0.19f));
  mouse_of(p, aim, reaction);
  return p;
}

double draw_reaction(const AimProfile& p, double u) {
  if (u < 0.5) return p.react_lo + (p.react_med - p.react_lo) * (u * 2);
  const double v = (u - 0.5) * 2;
  return p.react_med + (p.react_hi - p.react_med) * v * v;
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

void turn_axes_swim(int16_t& forward, int16_t& side, int16_t& up, Vec3f from, Vec3f to) {
  const ViewAxes a = angle_vectors({from.x, from.y, 0}), b = angle_vectors({to.x, to.y, 0});
  const float f = forward, s = side;
  const Vec3f wish = a.forward * f + a.right * s + Vec3f{0, 0, static_cast<float>(up)};
  const float nf = dot(wish, b.forward), ns = dot(wish, b.right);
  forward = quantize_axis(std::round(nf));
  side = quantize_axis(std::round(ns));
  up = quantize_axis(std::round(wish.z - nf * b.forward.z - ns * b.right.z));
}

}  // namespace ge::shooter
