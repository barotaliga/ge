// The player's view frame by frame (present.hpp).
//
// ge:summary The player's view frame by frame: the first-person eyes, the missiles and blasts
// ge:keywords present frame_view missiles blasts
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/present.hpp"

#include <algorithm>

#include "core/math.hpp"

#include "brush/map.hpp"
#include "camera/to_view.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {
constexpr double kDeg = 3.14159265358979323846 / 180;
}  // namespace

ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt) {
  // The mod's explosions since the last look (a rocket at point blank goes off in the frame that
  // fired it, so it is never seen in flight).
  if (frames > 0) missiles_of(l, v.missiles);
  if (l.explosions.size() < v.explosions_seen) v.explosions_seen = 0;  // a new map
  for (; v.explosions_seen < l.explosions.size(); v.explosions_seen++)
    v.blasts.push_back({engine_point(l.explosions[v.explosions_seen]), t});
  std::erase_if(v.blasts, [&](const Blast& b) { return t - b.t > kBlastS; });

  ge::camera::Subject s = subject_of(l, v.row);
  const PlayerState me = player_state(l);
  // The blows since the last look, each kicking the view away from its source.
  v.row.kick_pitch = v.row.kick_roll = kHitKickDeg * kDeg;
  if (l.blows.size() < v.blows_seen) v.blows_seen = 0;  // a new map
  for (; v.blows_seen < l.blows.size(); v.blows_seen++) {
    const Blow& b = l.blows[v.blows_seen];
    const ge::camera::Vec3 to = engine_point(me.origin), from = engine_point(b.from);
    ge::camera::Vec3 dir{to.x - from.x, to.y - from.y, to.z - from.z};
    const double len = ge::core::hypot3(dir.x, dir.y, dir.z);
    if (len > 0) dir = {dir.x / len, dir.y / len, dir.z / len};
    ge::camera::add_moment(s, ge::camera::Moment::hit,
                           std::min(1.0, (b.armor + b.blood) / kFullBlow), dir);
  }
  if (l.forced) ge::camera::add_moment(s, ge::camera::Moment::teleport, 0);
  if (s.grounded && !v.was_grounded)
    ge::camera::add_moment(s, ge::camera::Moment::landing,
                           -static_cast<double>(v.fall_speed) * ge::brush::kUnitM);
  v.was_grounded = s.grounded;
  v.fall_speed = me.velocity.z;
  ge::render::FrameView view;
  view.time_s = t;
  v.pose = ge::camera::frame_first_person(v.row, s, v.eyes, dt, 1.0);
  // The program's punch on the view (pitch down and yaw left positive in the map's angles).
  v.pose.pitch -= me.punch.x * kDeg;
  v.pose.yaw -= me.punch.y * kDeg;
  v.pose.roll += me.punch.z * kDeg;
  ge::camera::to_frame_view(v.pose, view);
  return view;
}

}  // namespace shooters_edge::client
