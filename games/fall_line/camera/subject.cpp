// A skier as the camera's subject, and the worn eyes off the figure.
//
// ge:summary A skier as the camera's subject, and the worn eyes off the figure
// ge:keywords fall-line camera subject skier worn-eyes
// ge:skill camera
// ge:test tests/fall_line_camera_test.cpp
#include "fall_line/camera/subject.hpp"

#include <array>

#include "core/math.hpp"
#include "fall_line/camera/rungs.hpp"
#include "skier/app/figure.hpp"

namespace fall_line::camera {

namespace cam = ge::camera;
namespace core = ge::core;

namespace {

// The body's lens: the figure's frame rolled by the lean (right side down is a negative turn
// about the forward axis, engine/core/quat.hpp). Down, he lies on his side.
core::Quat body_of(const ge::skier::SkierState& s, const ge::skier::World& world) {
  const core::Quat base = ge::skier::figure_frame(s, world).rot;
  const double roll = s.down ? 1.45 : s.lean;
  return core::multiply(base, core::from_axis_angle(0, 0, 1, -roll));
}

// The helmet's eye, m, in front of the head's centre and a little over it.
constexpr double kEyeAhead = 0.11, kEyeUp = 0.03;
// The tips' eye: a hand over the snow, ahead of the boots.
constexpr double kTipsUp = 0.3, kTipsAhead = 0.55;

}  // namespace

cam::Subject skier_subject(const ge::skier::SkierState& s, const ge::skier::World& world,
                           SubjectMemory& m) {
  cam::Subject out;
  out.pos = {s.x, s.y, s.z};
  out.vel = {s.vx, s.vy, s.vz};
  out.q = body_of(s, world);
  out.heading = s.heading;
  out.look_yaw = s.heading;
  out.grounded = s.on_snow;
  out.surface = s.on_snow ? s.packed : 0.0;
  if (m.framed) {
    const double moved = core::hypot3(s.x - m.x, s.y - m.y, s.z - m.z);
    if (moved > kTeleportM) {
      cam::add_moment(out, cam::Moment::teleport, 0);
    } else if (m.air && s.on_snow) {
      cam::add_moment(out, cam::Moment::landing, m.fall_mps);
    }
  }
  m.framed = true;
  m.air = !s.on_snow;
  m.fall_mps = s.vy < 0 ? -s.vy : 0.0;
  m.x = s.x;
  m.y = s.y;
  m.z = s.z;
  return out;
}

namespace {

// The head's centre, world, m.
core::Vec3 head_of(const ge::skier::SkierState& s, const ge::skier::World& world) {
  std::array<ge::skier::PiecePose, ge::skier::kFigurePieces> pieces;
  ge::skier::pose_figure(s, world, pieces);
  return pieces[14].centre;  // the helmet's ball (figure.cpp)
}

void fit_eyes(cam::Camera& c, const ge::skier::SkierState& s, const ge::skier::World& world,
              const core::Vec3& head) {
  const core::Quat q = body_of(s, world);
  core::Vec3 eye = core::unrotate(q, core::Vec3{head.x - s.x, head.y - s.y, head.z - s.z});
  eye.y += kEyeUp;
  eye.z += kEyeAhead;
  c.rigs[static_cast<int>(Rung::helmet)].mounted.eye_body = eye;
  c.rigs[static_cast<int>(Rung::tips)].mounted.eye_body = {0, kTipsUp + s.sink, kTipsAhead};
}

}  // namespace

void fit_worn(cam::Camera& c, const ge::skier::SkierState& s, const ge::skier::World& world) {
  fit_eyes(c, s, world, head_of(s, world));
}

RunCamera make_skier_camera(Rung start, uint32_t seed) {
  return RunCamera{make_run_camera(start, seed), SubjectMemory{}};
}

cam::Pose frame_camera(RunCamera& rc, const ge::skier::SkierState& s,
                       const ge::skier::World& skier_world, const cam::World& camera_world,
                       double dt) {
  const core::Vec3 head = head_of(s, skier_world);
  fit_eyes(rc.cam, s, skier_world, head);
  const cam::Subject subject = skier_subject(s, skier_world, rc.memory);
  cam::Pose p = cam::update(rc.cam, subject, camera_world, dt);
  const bool on_helmet = rung_of(rc.cam) == Rung::helmet && !cam::in_transition(rc.cam);
  const double d = core::hypot3(p.eye.x - head.x, p.eye.y - head.y, p.eye.z - head.z);
  if (!on_helmet && d < kHeadClip) {
    const double past = d + kHeadReach;
    p.near_m = past > p.near_m ? past : p.near_m;
  }
  // A worn lens on a body thrown down on its side would go under the snow with it.
  const double floor = cam::ground_at(camera_world, p.eye.x, p.eye.z) + kLensFloor;
  if (p.eye.y < floor) p.eye.y = floor;
  return p;
}

}  // namespace fall_line::camera
