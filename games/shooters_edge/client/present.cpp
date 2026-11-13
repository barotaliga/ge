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
#include "shooter/brush_hull.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {
constexpr double kDeg = 3.14159265358979323846 / 180;
}  // namespace

void add_view_camera_cvars(ge::console::Cvars& c, ge::camera::CameraCvars& out) {
  ge::camera::CameraDefaults d;  // the classic look's numbers, in map units
  d.unit_m = ge::brush::kUnitM;
  d.bob = 0.02;
  d.bobcycle = 0.6;
  d.bobup = 0.5;
  d.rollangle = 2;
  d.rollspeed = 200;
  d.kicktime = 0.5;
  d.kickpitch = d.kickroll = 0.6;
  d.idlescale = 0;
  d.landdip = 0;
  ge::camera::add_camera_cvars(c, out, d);
}

void apply_view_cvars(const ge::camera::CameraCvars& c, ViewState& v) {
  static const ge::camera::FirstPersonRow kClassic =
      ge::camera::classic_first_person(ge::brush::kUnitM);
  ge::camera::apply_first_person_cvars(c, kClassic, v.row);
  v.viewheight_m = c.viewheight * c.unit_m;
  if (c.fov > 0) v.row.fov_y = v.row.zoom_fov_y = ge::camera::fov_y_of(c.fov * kDeg, 4.0 / 3.0);
}

ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt) {
  // The mod's explosions since the last look (a rocket at point blank goes off in the frame that
  // fired it, so it is never seen in flight).
  if (frames > 0) missiles_of(l, v.missiles);
  if (l.explosions.size() < v.explosions_seen) v.explosions_seen = 0;  // a new map
  for (; v.explosions_seen < l.explosions.size(); v.explosions_seen++)
    v.blasts.push_back(
        {engine_point(l.explosions[v.explosions_seen]), t, l.explosions[v.explosions_seen]});
  std::erase_if(v.blasts, [&](const Blast& b) { return t - b.t > kBlastS; });

  ge::camera::Subject s = subject_of(l, v.row);
  if (v.viewheight_m > 0) v.row.eye_stand_m = v.viewheight_m;
  const PlayerState me = player_state(l);
  // The eye's place between the last two states, one server frame behind.
  if (l.server != nullptr) {
    const double now = l.server->sv.time;
    if (now != v.eye_now_s) {
      const float dx = me.origin.x - v.eye_now.x, dy = me.origin.y - v.eye_now.y,
                  dz = me.origin.z - v.eye_now.z;
      const bool cut = v.eye_now_s < 0 || now < v.eye_now_s || l.forced ||
                       dx * dx + dy * dy + dz * dz > kEyeSnapUnits * kEyeSnapUnits;
      v.eye_prev = cut ? me.origin : v.eye_now;
      v.eye_prev_s = cut ? now : v.eye_now_s;
      v.eye_now = me.origin;
      v.eye_now_s = now;
    }
    const double gap = v.eye_now_s - v.eye_prev_s;
    if (v.lerp && gap > 0 && gap <= 0.25) {
      const double tic = listen_tic(l);
      const double drawn = now + std::min(l.owed, tic) - tic;
      const auto f = static_cast<float>(std::clamp((drawn - v.eye_prev_s) / gap, 0.0, 1.0));
      sh::Vec3f feet{v.eye_prev.x + (v.eye_now.x - v.eye_prev.x) * f,
                     v.eye_prev.y + (v.eye_now.y - v.eye_prev.y) * f,
                     v.eye_prev.z + (v.eye_now.z - v.eye_prev.z) * f};
      feet.z += ge::shooter::kPlayerMins.z;
      s.pos = engine_point(feet);
    }
  }
  // The blows since the last look, each tipping the view toward its source by its count.
  if (l.blows.size() < v.blows_seen) v.blows_seen = 0;  // a new map
  for (; v.blows_seen < l.blows.size(); v.blows_seen++) {
    const Blow& b = l.blows[v.blows_seen];
    const ge::camera::Vec3 to = engine_point(me.origin), from = engine_point(b.from);
    ge::camera::Vec3 dir{to.x - from.x, to.y - from.y, to.z - from.z};
    const double len = ge::core::hypot3(dir.x, dir.y, dir.z);
    if (len > 0) dir = {dir.x / len, dir.y / len, dir.z / len};
    ge::camera::add_moment(s, ge::camera::Moment::hit,
                           std::max(kKickCountMin, 0.5 * b.armor + 0.5 * b.blood), dir);
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
  if (me.dead) v.pose.roll = kDeadRollDeg * kDeg;  // lying on the floor
  ge::camera::to_frame_view(v.pose, view);
  return view;
}

}  // namespace shooters_edge::client
