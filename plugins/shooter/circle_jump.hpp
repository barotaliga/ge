// THE CIRCLE JUMP: a hop's first jump taken out of a curve on the ground, as people take theirs.
//
// The ground's move (pmove.cpp): friction takes `friction` x v x dt a command (v past
// `stopspeed`), then the wish adds along itself up to `accelerate` x maxspeed x dt (41.6 at 13 ms)
// but only while the velocity's share along the wish is under maxspeed. With the wish along the
// velocity a run is capped at maxspeed (320). Held at an angle off the velocity the run settles
// faster (50 degrees: about 465), and the best angle each command, acos((maxspeed - add) / v'),
// with v' the speed after friction, takes a run from 320 to 440 in 0.13 s and 477 in 0.26 s while
// the velocity turns about 4 degrees a command toward the wish's side.
//
// People do it (12 dm2 matches read frame by frame, se-moves `--track`): their standing jumps that
// take off at 400-480 come out of a curve: the speed 347 -> 382 -> 424 over the last 0.3 s, the
// heading turning 51-75 degrees, the view on the velocity: forward and a side key, whose wish lies
// 45 degrees off the view.
//
// `circle_step`, one command on the ground: the side key that turns the velocity toward the route
// (the aim `err` degrees off the velocity, + to the left), the view that puts the two keys' wish at
// the best angle on that side, and whether to jump now: at `target` speed, after `most_s`, or once
// the velocity has turned `past` degrees beyond the aim (the curve brought it round). A curve is
// begun only into a turn (the aim `corner` degrees off): on a straight way it would leave the line
// for the walls, and people's curves are their corners. Pure and deterministic (core's libm).
//
// ge:summary The circle jump: a hop's first jump out of a curve on the ground, the run past 320
// ge:keywords circle-jump ground-strafe prestrafe strafe-run hop-start takeoff-speed circle_step
// ge:skill shooter bots
// ge:test tests/shooter_circle_jump_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pmove.hpp"

namespace ge::shooter {

struct CircleSettings {
  float target = 430;   // jump once the run is this fast, units/s
  float most_s = 0.3f;  // or after this long on the curve
  float past = 5;       // or once the velocity has turned this far past the aim, degrees
  float corner = 25;    // curved only into a turn: the aim at least this far off at the start
};

struct CircleStep {
  bool jump = false;  // jump on this command (the keys below unused)
  float yaw = 0;      // the view: the wish of forward and `side` at the best angle, degrees
  int8_t side = 0;    // the side key: -1 left, 1 right
};

// One command of the curve, `elapsed_s` into it: `heading` the velocity's (degrees, + left),
// `speed` across, `err` the aim off the velocity (+ left), `side` the key held so far (0 none).
CircleStep circle_step(float heading, float speed, float err, float elapsed_s, float dt,
                       int8_t side, const MoveSettings& move, const CircleSettings& set);

// The wish's best angle off the velocity on the ground for the most speed after this command,
// degrees (0 below the speed where the full wish along it still adds).
float circle_angle(float speed, float dt, const MoveSettings& move);

}  // namespace ge::shooter
