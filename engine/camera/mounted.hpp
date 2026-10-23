// THE MOUNTED RIG: a lens worn by the body (the tips of the skis, a helmet, a bumper, a hood, a
// cockpit seat), a point in the body's own frame looking along its forward axis `look_m` ahead
// and `down` rad under it, pitching with the body and rolling with it by `roll_share`.
//
// Bolted rigidly it is a still photograph on smooth ground, so A HEAD ON A NECK carries it: the
// eye is pushed off upright by the body's ACCELERATION (thrown forward under the brakes, back
// under power, out of a corner, down into a dip), read by differencing VELOCITIES, never
// positions: a position difference is dominated by the speed, so a frame that takes two steps
// beside one that takes none hands the neck metres a second of motion nothing asked for; a
// velocity difference is bounded by the acceleration. The reading is smoothed by a critically
// damped spring (`neck_halflife_s`) before the neck sees it, and that smoothing is the head's
// inertia: what the body does faster is carried rigidly, which reads as the body moving rather
// than the camera shaking. The neck's travel is bounded (`neck_max_m`, `neck_max_rad`). A hit
// throws the head with an impulse away from the blow. Under all of it a fine GRAIN of the
// surface (`Subject::surface`) as motion.
//
// `motion` (comfort, 0..1) scales the neck, the grain and the roll's share.
//
// ge:summary The mounted (worn) rig: a lens in the body frame with a head on a neck and grain
// ge:keywords camera mounted worn bolted cockpit helmet bumper hood tips onboard head neck
// ge:keywords inertia acceleration lean roll-share grain frame_mounted
// ge:skill camera
// ge:test tests/camera_mounted_test.cpp
#pragma once

#include "camera/follow.hpp"
#include "camera/pose.hpp"
#include "camera/shake.hpp"
#include "camera/subject.hpp"

namespace ge::camera {

struct MountedRow {
  Vec3 eye_body{0, 0.72, 0.08};  // m, body frame: x right, y up, z forward (over the origin)
  double look_m = 30;
  double down = 0;  // rad the look is pitched under the forward axis
  double fov_y = 1.19;
  double fov_per_mps = 0.006;
  double roll_share = 0.6;
  double neck_halflife_s = 0.08;
  double neck_m_per_mps2 = 0.004;  // eye travel a m/s^2 of the body's acceleration
  double neck_max_m = 0.04;
  double neck_rad_per_mps2 = 0.002;  // head tip a m/s^2
  double neck_max_rad = 0.05;
  double hit_mps = 0.6;  // the neck's velocity a unit of a hit's size
  double grain_m = 0.002;
  double grain_hz = 11;
};

struct MountedState {
  bool fresh = true;
  Vec3 last_vel{0, 0, 0};
  Spring ax{}, ay{}, az{};  // the body-frame acceleration, smoothed
  Spring hx{}, hy{}, hz{};  // the head's own offset after a hit, m
  double clock = 0;
  Waves grain = make_waves(0x9a17);
};

Pose frame_mounted(const MountedRow& row, const Subject& s, MountedState& st, double dt,
                   double motion);

}  // namespace ge::camera
