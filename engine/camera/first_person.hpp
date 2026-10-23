// THE FIRST-PERSON RIG: the eyes of a body that walks, runs, crouches, jumps and is shot at. The
// angles are the player's (`Subject::look_yaw`, `look_pitch`), taken as they are; everything this
// rig adds is presentation, never fed back to the simulation:
//
//   VIEW HEIGHT    the eyes over the body's origin, eased between standing and crouched.
//   STEP SMOOTHING a body stepping up a stair rises at once in the simulation; the eyes rise at
//                  `step_rise_mps` and never lag more than `step_max_m`, so a staircase is a ramp
//                  to the eye. Down is taken at once (a fall is not smoothed).
//   THE BOB        the stride as a cycle in time: `bob_cycle_s` long, the first `bob_up` of it
//                  rising (half a sine) and the rest falling, its height `bob` metres per m/s of
//                  ground speed, a third of it held as a lift and the rest swung, clamped to
//                  `bob_lo_m..bob_hi_m`; none in the air.
//   THE ROLL       a strafe tips the horizon: `roll_angle` at `roll_speed_mps` of sideways speed,
//                  in proportion below it, capped above.
//   THE KICK       a hit kicks the view away from where it came from: its side into the roll and
//                  its front-back into the pitch, by `kick_roll` and `kick_pitch` a unit of size,
//                  falling off linearly over `kick_time_s`. A shot fired punches the pitch up on a
//                  stiff spring (`punch`, `punch_poles`) that springs back.
//   THE DIP        a landing pushes the eyes down by `dip_per_mps` a m/s of fall speed (on a
//                  follower that springs back, soft-clipped to `dip_max_m`).
//   THE ZOOM       the fov eased from `fov_y` to `zoom_fov_y` as `Subject::zoom` rises.
//   IDLE SWAY      a slow breathing drift of the angles, `idle` rad at 1; off by default.
//
// Every number is a row a game sets (its units converted to metres once) and, through the
// camera's settings, a cvar a player may change. `motion` (the player's comfort, 0..1) scales
// the bob, the roll, the kick, the dip and the sway; the look itself is never scaled.
//
// ge:summary The first-person rig: view height, step smoothing, bob, strafe roll, kick, dip, zoom
// ge:keywords camera first-person fps eyes view-height viewheight crouch step-smoothing stairs
// ge:keywords bob bobbing view-bob bob-cycle bobup strafe-roll roll-angle roll-speed kick punch
// ge:keywords view-punch damage-kick landing-dip land zoom idle-sway breathing fov
// ge:keywords frame_first_person first_person_bob strafe_roll
// ge:skill camera
// ge:test tests/camera_first_person_test.cpp
#pragma once

#include "camera/follow.hpp"
#include "camera/pose.hpp"
#include "camera/subject.hpp"

namespace ge::camera {

struct FirstPersonRow {
  double eye_stand_m = 1.62;
  double eye_crouch_m = 1.0;
  double crouch_halflife_s = 0.05;
  double step_rise_mps = 2.0;  // how fast the eyes climb a step
  double step_max_m = 0.3;     // the most they lag the body
  double bob = 0.02;           // m of bob per m/s of ground speed
  double bob_cycle_s = 0.6;
  double bob_up = 0.5;
  double bob_lo_m = -0.18, bob_hi_m = 0.10;
  double roll_angle = 0.0349;   // rad (2 deg)
  double roll_speed_mps = 5.0;  // the sideways speed that reaches it
  double kick_pitch = 0.0105;   // rad of pitch a unit of a hit's size (0.6 deg)
  double kick_roll = 0.0105;
  double kick_time_s = 0.5;
  double punch = 1.5;  // rad/s of pitch-up velocity a unit of a shot's size
  Poles punch_poles{6.0, 0.55, 0.0};
  double dip_per_mps = 0.012;  // m of dip per m/s of fall speed
  double dip_max_m = 0.25;
  Poles dip_poles{2.4, 0.7, 0.0};
  double fov_y = 1.287;  // vertical, rad (90 deg horizontal at 4:3)
  double zoom_fov_y = 0.55;
  double zoom_halflife_s = 0.06;
  double idle = 0.0;  // rad of sway at 1 (0: off)
};

struct FirstPersonState {
  bool fresh = true;
  double clock = 0;       // the bob's own clock, s
  double eye_base = 0;    // the smoothed body height under the eyes, m
  double eye_height = 0;  // the smoothed view height, m
  bool was_grounded = true;
  double kick_pitch = 0, kick_roll = 0, kick_left = 0;  // the hit kick and its time left
  Follower punch{};
  Follower dip{};
  double zoom = 0;
};

// The bob at clock `t_s` for a ground speed, m (the formula alone, for tests and labs).
double first_person_bob(const FirstPersonRow& row, double t_s, double ground_speed);
// The strafe roll for a sideways speed (positive: moving right), rad.
double strafe_roll(const FirstPersonRow& row, double side_speed);

// One frame of the rig. `motion` is the comfort scale (0..1).
Pose frame_first_person(const FirstPersonRow& row, const Subject& s, FirstPersonState& st,
                        double dt, double motion);

}  // namespace ge::camera
