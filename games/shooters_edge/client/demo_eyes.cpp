// The classic view of a recording or a game joined (demo_eyes.hpp).
//
// ge:summary The classic bob, strafe roll and damage kick on a demo's or a joined server's eye
// ge:keywords demo_eyes_view demo_eyes_drawn ground-trace demo_eyes_kick
// ge:skill camera shooter
// ge:test tests/shooters_edge_demo_eyes_test.cpp
#include "shooters_edge/client/demo_eyes.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"

#include "brush/map.hpp"
#include "camera/pose.hpp"
#include "camera/to_view.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180;
constexpr double kGibEyeUnits = 8;  // the eye over the origin gibbed, units

// The player the eye rides in `s`: present, never the intermission's view. -1: none.
int rider(const DemoState& s) {
  if (s.intermission) return -1;
  if (s.format == DemoFormat::single)
    return s.view_entity >= 0 && static_cast<size_t>(s.view_entity) < s.entities.size()
               ? s.view_entity
               : -1;
  return s.player >= 0 && static_cast<size_t>(s.player) < s.players.size() &&
                 s.players[static_cast<size_t>(s.player)].present
             ? s.player
             : -1;
}

sh::Vec3f feet_of(const DemoState& s) {
  sh::Vec3f f = s.eye;
  f.z += ge::shooter::kPlayerMins.z - s.view_height;
  return f;
}

// The player's box on a floor: traced `kGroundProbe` units down through the player hull.
bool on_floor(const sh::BrushModel& world, sh::Vec3f origin) {
  const sh::Hull& hull = world.hulls[1];
  if (hull.nodes == nullptr || hull.planes == nullptr) return false;
  const sh::HullTrace tr =
      sh::trace_hull(hull, origin, {origin.x, origin.y, origin.z - kGroundProbe});
  return !tr.startsolid && tr.fraction < 1 && tr.plane.normal.z >= kFloorNormal;
}

}  // namespace

void demo_eyes_kick(const DemoState& read, DemoEyes& e, double dt) {
  if (read.kicks_taken < e.kicks_seen) e.kicks_seen = read.kicks_taken;  // the demo opened again
  if (read.kicks_taken != e.kicks_seen) e.kick_to = -static_cast<float>(read.last_kick);
  e.kicks_seen = read.kicks_taken;
  const float rate = e.kick_to == 0 ? kKickBack : e.kick_to < -2 ? kKickOutBig : kKickOutSmall;
  const float step = rate * static_cast<float>(dt);
  if (e.kick > e.kick_to) {
    e.kick = std::max(e.kick - step, e.kick_to);
  } else {
    e.kick = std::min(e.kick + step, e.kick_to);
  }
  if (e.kick == e.kick_to) e.kick_to = 0;
}

bool demo_eyes_drawn(const DemoState& s) { return s.format != DemoFormat::q2 && rider(s) >= 0; }

ge::render::FrameView demo_eyes_view(const DemoState& drawn, const DemoState& read, DemoEyes& e,
                                     const ge::camera::FirstPersonRow& row_in,
                                     const sh::BrushModel* world, const DemoBody& body,
                                     double fov_y, double clock_s, double time_s) {
  if (!demo_eyes_drawn(drawn)) {
    e.clock = -1;  // the next one drawn starts over
    ge::render::FrameView v = demo_view(drawn, fov_y, time_s);
    demo_view_behind(drawn, e.behind, world, v);
    return v;
  }
  // A seek back, a new level or another player: the rig and the estimate start over.
  const bool cut = e.clock < 0 || clock_s < e.clock || drawn.levels != e.levels ||
                   drawn.player != e.player || read.frames < e.frames_seen;
  if (cut) {
    const bool* gun_kick = e.gun_kick;
    const float behind = e.behind;
    e = DemoEyes{};
    e.gun_kick = gun_kick;
    e.behind = behind;
    e.kicks_seen = read.kicks_taken;
    e.levels = drawn.levels;
    e.player = drawn.player;
    e.blows_seen = read.blows_taken;  // what was taken before is not shown
  }
  const double dt = cut ? 0 : std::min(clock_s - e.clock, kMostEyesStep);
  e.clock = clock_s;

  // The velocity off the frames read, and the blows a frame read carried, once each.
  const bool fresh = read.frames != e.frames_seen;
  e.frames_seen = read.frames;
  ge::camera::Subject s;
  if (fresh && rider(read) >= 0) {
    const sh::Vec3f f = feet_of(read);
    if (e.feet_time >= 0 && read.time > e.feet_time) {
      const sh::Vec3f d = f - e.feet;
      const float span = static_cast<float>(read.time - e.feet_time);
      e.velocity = d.x * d.x + d.y * d.y + d.z * d.z > kMostStepUnits * kMostStepUnits
                       ? sh::Vec3f{}
                       : d * (1.0f / span);
    }
    if (read.time != e.feet_time) {
      e.feet = f;
      e.feet_time = read.time;
    }
  }
  const sh::Vec3f velocity = body.known            ? body.velocity
                             : drawn.velocity_read ? drawn.velocity
                                                   : e.velocity;
  const sh::Vec3f feet = feet_of(drawn);
  sh::Vec3f origin = feet;
  origin.z -= ge::shooter::kPlayerMins.z;
  s.pos = engine_point(feet);
  s.vel = engine_point(velocity);
  s.grounded = body.known         ? body.on_ground
               : world != nullptr ? on_floor(*world, origin)
                                  : velocity.z == 0;
  s.look_yaw = engine_heading(drawn.angles.y);
  s.look_pitch = engine_pitch(drawn.angles.x);
  s.heading = s.look_yaw;

  ge::camera::FirstPersonRow row = row_in;
  const DemoEntity* me = drawn.format == DemoFormat::single
                             ? nullptr
                             : &drawn.players[static_cast<size_t>(drawn.player)];
  const bool gib = me != nullptr && me->gib;
  const bool dead = me != nullptr ? me->dead && !gib : drawn.stats[kStatHealth] <= 0;
  const double over = gib ? kGibEyeUnits : dead ? kDeadEyeUnits : drawn.view_height;
  row.eye_stand_m = row.eye_crouch_m = (over - ge::shooter::kPlayerMins.z) * ge::brush::kUnitM;

  if (read.blows_taken < e.blows_seen) e.blows_seen = read.blows_taken;  // the demo opened again
  const int64_t unseen =
      std::min<int64_t>(read.blows_taken - e.blows_seen, static_cast<int64_t>(read.blows.size()));
  e.blows_seen = read.blows_taken;
  for (size_t k = read.blows.size() - static_cast<size_t>(unseen); k < read.blows.size(); k++) {
    const DemoBlow& b = read.blows[k];
    const ge::camera::Vec3 to = engine_point(origin), from = engine_point(b.from);
    ge::camera::Vec3 dir{to.x - from.x, to.y - from.y, to.z - from.z};
    const double len = ge::core::hypot3(dir.x, dir.y, dir.z);
    if (len > 0) dir = {dir.x / len, dir.y / len, dir.z / len};
    ge::camera::add_moment(s, ge::camera::Moment::hit,
                           std::max(kKickCountMin, 0.5 * b.armor + 0.5 * b.blood), dir);
  }
  if (cut) ge::camera::add_moment(s, ge::camera::Moment::teleport, 0);
  if (s.grounded && !e.was_grounded)
    ge::camera::add_moment(s, ge::camera::Moment::landing,
                           -static_cast<double>(e.fall_speed) * ge::brush::kUnitM);
  e.was_grounded = s.grounded;
  e.fall_speed = velocity.z;

  ge::camera::Pose pose = ge::camera::frame_first_person(row, s, e.eyes, dt, 1.0);
  demo_eyes_kick(read, e, dt);
  if (e.gun_kick != nullptr && *e.gun_kick &&
      e.kick != 0) {  // along the view by the kick, pitched up by half of it
    const ge::camera::Vec3 f = ge::camera::look_forward(pose.yaw, pose.pitch);
    const double k = e.kick * ge::brush::kUnitM;
    pose.eye = {pose.eye.x + k * f.x, pose.eye.y + k * f.y, pose.eye.z + k * f.z};
    pose.pitch -= 0.5 * e.kick * kDeg;
  }
  if (dead || gib) pose.roll = kDeadRollDeg * kDeg;  // lying on the floor
  ge::render::FrameView view;
  ge::camera::to_frame_view(pose, view);
  view.fov_y = fov_y;
  view.time_s = time_s;
  demo_view_behind(drawn, e.behind, world, view);
  return view;
}

void demo_view_behind(const DemoState& s, float units, const sh::BrushModel* world,
                      ge::render::FrameView& view) {
  if (!(units > 0) || s.intermission || s.format == DemoFormat::single || s.player < 0 ||
      static_cast<size_t>(s.player) >= s.players.size() ||
      !s.players[static_cast<size_t>(s.player)].present)
    return;
  // The way the player looks, in map units (yaw counter-clockwise from +x, pitch down positive).
  const double yaw = s.angles.y * kDeg, pitch = s.angles.x * kDeg;
  const sh::Vec3f back{static_cast<float>(-std::cos(pitch) * std::cos(yaw)),
                       static_cast<float>(-std::cos(pitch) * std::sin(yaw)),
                       static_cast<float>(std::sin(pitch))};
  const sh::Vec3f from = s.eye;
  float reach = units;
  if (world != nullptr) {
    const sh::Hull& hull = world->hulls[0];
    if (hull.nodes != nullptr && hull.planes != nullptr) {
      const sh::HullTrace tr = sh::trace_hull(hull, from, from + back * (units + kBehindWallGap));
      if (!tr.startsolid && tr.fraction < 1)
        reach = std::max(0.0f, tr.fraction * (units + kBehindWallGap) - kBehindWallGap);
    }
  }
  const ge::camera::Vec3 at = engine_point(from + back * reach);
  const ge::camera::Vec3 eye = engine_point(from);
  // The rig's bob and roll ride along: the camera keeps its offset from the eye it was given.
  view.eye[0] += at.x - eye.x;
  view.eye[1] += at.y - eye.y;
  view.eye[2] += at.z - eye.z;
}

}  // namespace shooters_edge::client
