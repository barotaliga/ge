// WHERE THE HELD MODEL IS DRAWN: the first-person weapon's place in the VIEW each display frame,
// from the player's look and stride and what just happened (a shot, a landing, a weapon put away).
// Presentation only: nothing here is fed back to the simulation, and it reads no wall clock.
//
// The held model (viewmodel/held.hpp) has its grip at its origin and its muzzle along +z. The
// placement is an affine from that frame into the view's (+x right, +y up, +z forward, m):
//
//   REST           the grip at `offset_m` from the eye: to the right, down and forward.
//   THE BOB        the eye's own bob (camera/first_person.hpp's, handed in as `bob_m`) moves the
//                  view, and the gun with it; on top of that the gun is pushed forward
//                  `bob_forward` and up `bob_up` of the bob, so it rides into and out of the
//                  picture as the body strides. `bob_side` swings it across, a figure of eight
//                  over two strides, its width that share of the ground speed in metres (m per
//                  m/s), none in the air.
//   THE SWAY       the gun's aim follows the look on a second-order follower (`sway_poles`): a
//                  turn leaves it behind and it catches up with a little overshoot, so it has
//                  weight. Its lag is capped at `sway_max` rad and also slides the grip across by
//                  `sway_shift` m a radian.
//   THE KICK       a shot pushes the grip back (`kick_back` m/s a unit of size) and the muzzle up
//                  (`kick_pitch` rad/s), each on a stiff spring (`kick_poles`) that settles it.
//   THE DIP        a landing pushes the gun down `dip_per_mps` m/s a m/s of fall, on `dip_poles`.
//   PUT AWAY       `lowered` (0 up, 1 away) is chased at 1 / `switch_s` a second; away, the gun
//                  is `lower_m` lower and pitched down by `lower_pitch`.
//   IDLE           a slow breathing drift of `idle_m` at `idle_hz`, fading out with speed.
//
// `motion` (the player's comfort, 0..1) scales every motion the player did not command, as the
// camera's does. The gun has its own lens, `fov_y` (0: the view's), so a wide view does not
// stretch it; the renderer draws it in its own depth range over the world (it never sinks into
// a wall).
//
// ge:summary The held weapon's place in the view: offset, bob, sway, kick, dip, put-away, idle
// ge:keywords viewmodel view-model first-person weapon gun held place placement offset bob
// ge:keywords gun-bob weapon-bob sway weapon-sway lag inertia kick recoil dip landing switch
// ge:keywords lower raise put-away idle breathing viewmodel-fov frame_viewmodel placement-row
// ge:keywords held_to_world held_dir_to_world muzzle-world lens-match
// ge:skill render camera
// ge:test tests/viewmodel_place_test.cpp
#pragma once

#include "camera/follow.hpp"
#include "camera/pose.hpp"

namespace ge::viewmodel {

struct PlaceRow {
  double offset_m[3] = {0.13, -0.16, 0.2};  // the grip from the eye: right, up, forward
  double fov_y = 0;                         // the gun's own vertical lens, rad (0: the view's)
  double bob_forward = 0.4;                 // share of the eye's bob, forward
  double bob_up = 0.0;                      // and up
  double bob_side = 0.0;                    // m of swing across a m/s of ground speed
  camera::Poles sway_poles{5.0, 0.6, 0.0};
  double sway_max = 0.12;   // rad
  double sway_shift = 0.0;  // m across a radian of lag
  double kick_back = 1.2;   // m/s a unit of a shot
  double kick_pitch = 2.5;  // rad/s a unit of a shot
  camera::Poles kick_poles{7.0, 0.7, 0.0};
  double dip_per_mps = 0.02;
  camera::Poles dip_poles{3.0, 0.6, 0.0};
  double switch_s = 0.2;
  double lower_m = 0.25;
  double lower_pitch = 0.6;  // rad, muzzle down
  double idle_m = 0.0;
  double idle_hz = 0.25;
};

// THE TWO ROWS a game starts from: the classic arena feel (the gun rides forward with the bob and
// lags the look a little; nothing swings across) and a modern one (a figure-eight stride sway,
// more weight on a turn, a breath at rest). A game copies one and changes what it likes.
PlaceRow classic_row();
PlaceRow modern_row();

struct PlaceInput {
  double look_yaw = 0, look_pitch = 0;  // rad, the player's look (as the camera takes it)
  double bob_m = 0;                     // the eye's bob this frame
  double stride_s = 0;                  // the bob's clock, s
  double stride_cycle_s = 0.6;          // one stride
  double ground_speed = 0;              // m/s
  bool grounded = true;
  double shot = 0;     // a shot fired this frame: its size (0: none)
  double landing = 0;  // a landing this frame: its fall speed, m/s (0: none)
  double lowered = 0;  // the target: 0 held up, 1 put away
};

struct PlaceState {
  bool fresh = true;
  camera::Follower yaw{}, pitch{};  // the gun's aim chasing the look
  camera::Follower back{}, rise{};  // the kick
  camera::Follower dip{};
  double lowered = 0;
  double clock = 0;  // the idle drift's own clock, s
};

// THE PLACEMENT: the held frame into the view's, three rows of a 3x4 matrix (p' = M (x, y, z, 1)),
// and the lens the gun is drawn with.
struct Placement {
  double m[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  double fov_y = 0;  // rad; 0: the view's
  // The pieces it was made of, for a lab's chart: the grip's offset from rest (right, up,
  // forward, m) and the turn (yaw right, pitch up, roll right side down, rad).
  double shift_m[3] = {0, 0, 0};
  double turn[3] = {0, 0, 0};
};

// One display frame of `dt` seconds. `motion` is the comfort scale (0..1).
Placement frame_viewmodel(const PlaceRow& row, const PlaceInput& in, PlaceState& st, double dt,
                          double motion = 1.0);

// A point of the held frame into the view's by a placement.
void place_point(const Placement& p, const float in[3], float out[3]);
// A direction (a normal) into the view's.
void place_vector(const Placement& p, const float in[3], float out[3]);

// A point of the held frame (the muzzle's tip) into the WORLD, where an effect drawn in the
// world's pass (a muzzle flash, its smoke, the shot's light) lines up with the gun drawn through
// its own lens: placed into the view, then moved across at its depth so it lands on the same
// screen ray under the view's lens (x and y times tan(view.fov_y / 2) / tan(gun_fov_y / 2)).
// `gun_fov_y` is the lens the gun is drawn with (the placement's, or 0: the view's, no change).
void held_to_world(const Placement& p, double gun_fov_y, const camera::Pose& view,
                   const float held[3], double out[3]);
// A direction of the held frame (the muzzle's) into the world through the same lens, unit: the
// way from the tip's world point to the world point of the tip moved a step along it.
void held_dir_to_world(const Placement& p, double gun_fov_y, const camera::Pose& view,
                       const float held_at[3], const float held_dir[3], double out[3]);

}  // namespace ge::viewmodel
