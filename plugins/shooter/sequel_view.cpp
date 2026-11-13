// The sequel's view as its server works it out (sequel_view.hpp).
//
// ge:summary The sequel's view by the server: bob, run tilt, blow and fall kicks, the view offset
// ge:keywords sequel view bob kick-angles view-offset step_sequel_view
// ge:skill shooter camera
// ge:test tests/shooter_sequel_view_test.cpp
#include "shooter/sequel_view.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::shooter {

namespace {

constexpr float kKickTime = 0.5f, kFallTime = 0.3f;
constexpr float kBobMost = 6;  // units of lift
constexpr float kKickMost = 50, kKickShare = 0.3f;

}  // namespace

void sequel_view_blow(SequelViewState& st, int points, int push, Vec3f to_source) {
  if (points <= 0 && push <= 0) return;
  st.blow_count += std::max(points, 0);
  st.blow_push += std::abs(push);
  if (length(to_source) > 0) st.blow_way = to_source;
}

void sequel_view_hard_kick(SequelViewState& st, float pitch, float roll) {
  st.kick_pitch = pitch;
  st.kick_roll = roll;
  st.kick_left = kKickTime;
}

void sequel_view_fall(SequelViewState& st, float severity) {
  if (severity < 15) return;
  st.fall_value = std::min(severity * 0.5f, 40.0f);
  st.fall_left = kFallTime;
}

SequelView step_sequel_view(SequelViewState& st, const SequelViewSettings& set,
                            const SequelViewIn& in) {
  const ViewAxes ax = angle_vectors(in.view_angles);
  // The frame's blows: a kick when they pushed and he lives.
  if (st.blow_count > 0) {
    const float count = static_cast<float>(std::max(st.blow_count, 10));
    if (st.blow_push > 0 && !in.dead && in.health > 0) {
      float kick = static_cast<float>(st.blow_push) * 100 / in.health;
      kick = std::min(std::max(kick, count * 0.5f), kKickMost);
      Vec3f way = st.blow_way;
      normalize(way);
      st.kick_roll = kick * dot(way, ax.right) * kKickShare;
      st.kick_pitch = -kick * dot(way, ax.forward) * kKickShare;
      st.kick_left = kKickTime;
    }
  }
  st.blow_count = st.blow_push = 0;
  st.blow_way = {};

  // The stride.
  const float speed = root_of(in.velocity.x * in.velocity.x + in.velocity.y * in.velocity.y);
  if (speed < 5) {
    st.bob_time = 0;
    st.bob_step = 0;
  } else if (in.on_ground) {
    st.bob_step = speed > 210 ? 0.25f : speed > 100 ? 0.125f : 0.0625f;
  }
  st.bob_time += st.bob_step;
  const float clock = in.ducked ? st.bob_time * 4 : st.bob_time;
  const bool odd = (static_cast<int>(clock) & 1) != 0;
  const auto share = static_cast<float>(
      std::fabs(core::libm::sin(static_cast<double>(clock) * 3.14159265358979323846)));

  SequelView out;
  // The angles: the shot's, the blow's, the fall's, the run's and the bob's.
  if (!in.dead) {
    out.kick = st.weapon_angles;
    if (st.kick_left > 0) {
      const float f = st.kick_left / kKickTime;
      out.kick.x += st.kick_pitch * f;
      out.kick.z += st.kick_roll * f;
    }
    if (st.fall_left > 0) out.kick.x += st.fall_value * (st.fall_left / kFallTime);
    out.kick.x += dot(in.velocity, ax.forward) * set.run_pitch;
    out.kick.z += dot(in.velocity, ax.right) * set.run_roll;
    const float duck = in.ducked ? 6.0f : 1.0f;
    out.kick.x += share * set.bob_pitch * speed * duck;
    out.kick.z += (odd ? -1.0f : 1.0f) * share * set.bob_roll * speed * duck;
  }
  // The eye: the view height, the fall's dip, the bob's lift, the shot's push.
  out.offset.z = in.view_height;
  if (st.fall_left > 0) out.offset.z -= st.fall_value * (st.fall_left / kFallTime) * 0.4f;
  out.offset.z += std::min(share * speed * set.bob_up, kBobMost);
  out.offset = out.offset + st.weapon_origin;
  out.offset.x = std::clamp(out.offset.x, -14.0f, 14.0f);
  out.offset.y = std::clamp(out.offset.y, -14.0f, 14.0f);
  out.offset.z = std::clamp(out.offset.z, -22.0f, 30.0f);

  // Time on: the kicks fade, the shot is spent.
  if (st.kick_left > 0) st.kick_left = std::max(st.kick_left - kSequelViewFrame, 0.0f);
  if (st.kick_left < 1e-4f) st.kick_left = 0;  // a rounding's crumb is no frame
  if (st.kick_left <= 0) st.kick_pitch = st.kick_roll = 0;
  if (st.fall_left > 0) st.fall_left = std::max(st.fall_left - kSequelViewFrame, 0.0f);
  if (st.fall_left < 1e-4f) st.fall_left = 0;
  if (st.fall_left <= 0) st.fall_value = 0;
  st.weapon_angles = st.weapon_origin = {};
  return out;
}

}  // namespace ge::shooter
