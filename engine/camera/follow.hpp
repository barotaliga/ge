// FOLLOWERS: how every reading a camera chases gets weight. A rig never copies its target and
// never eases it by a bare exponential (which moves fastest the instant the target jumps and
// reads as a lens with no weight); it runs it through one of these.
//
// THE SECOND-ORDER FOLLOWER (`Poles`): a mass on a damped spring placed by its two poles,
// s = w(-zeta +- sqrt(zeta^2 - 1)), w = 2 pi f:
//   - `f_hz`, the natural frequency: how quickly it answers at all;
//   - `zeta`, the damping ratio: under 1 overshoots and swings back (a horizon that breathes
//     after a landing), 1 critically damped (the quickest settle with no overshoot), over 1
//     creeps in;
//   - `r`, the response: 0 starts from rest, 1 meets a step at once, 2 tracks a steady ramp with
//     no lag, over 1 anticipates, under 0 winds up against the move first.
// Solved EXACTLY over each frame for the target held over it (the closed form of each damping
// case), so a long frame on a slow device draws the path a short one would have, and no frame
// length can make it ring or explode. `camera-follow` (lab) holds the paths at 30..240 Hz and
// through hitches to one another.
//
// THE CRITICALLY DAMPED SPRING BY HALF-LIFE (`Spring`, `spring_to`): one number, the time to
// halve the distance, solved exactly over the step (so it is the same at every frame rate to
// rounding); and `damp`, the first-order exact ease by half-life, for a reading with no character.
//
// THE DEAD BAND (`dead_band`): the share of an error a follower chases: nothing inside `band`,
// all of it past `band + soft`, a smooth ramp between, so the band's edge is never felt.
//
// ge:summary Camera followers: second-order (f, zeta, r), half-life spring and damp, dead band
// ge:keywords camera follower spring damper second-order poles zeta response critically-damped
// ge:keywords half-life halflife damp ease inertia weight dead-band dead-zone soft-zone
// ge:keywords frame-rate-independent follow follow_angle follow_trend settle spring_to
// ge:skill camera
// ge:test tests/camera_follow_test.cpp
#pragma once

namespace ge::camera {

struct Poles {
  double f_hz = 1.0;  // the natural frequency, Hz
  double zeta = 1.0;  // the damping ratio
  double r = 0.0;     // the response: 0 from rest, 1 meets a step, 2 tracks a ramp
};

// One second-order follower's memory: where it is, how fast it moves, and the target it was
// handed last (for the target's own velocity).
struct Follower {
  double y = 0, v = 0, x = 0;
};

// Put the follower AT `at`, at rest (a new run, a reset, a teleport). Returns `at`.
double settle(Follower& s, double at);
// Advance toward `target` by `dt` s; the target's velocity is the difference of two frames.
double follow(Follower& s, const Poles& p, double target, double dt);
// The same with the target's velocity given (a TREND, so `r` leads the drift, not every bump).
double follow_trend(Follower& s, const Poles& p, double target, double target_rate, double dt);
// For an ANGLE, rad: the target taken the short way round; the follower may wind past +-pi.
double follow_angle(Follower& s, const Poles& p, double target, double dt);
// Push the follower: add `dv` to its velocity (a kick it springs back from).
inline void kick(Follower& s, double dv) { s.v += dv; }

// A critically damped spring by half-life: its position and velocity.
struct Spring {
  double x = 0, v = 0;
};
// Advance toward `target` over `dt` with `halflife_s` (0 or less: there at once). Exact.
double spring_to(Spring& s, double target, double halflife_s, double dt);
// `x` eased toward `target` by a first-order half-life over `dt`. Exact.
double damp(double x, double target, double halflife_s, double dt);

// The share (0..1) of an error of size |err| a follower chases: 0 within `band`, 1 past
// `band + soft`, smoothstep between.
double dead_band(double err, double band, double soft);

}  // namespace ge::camera
