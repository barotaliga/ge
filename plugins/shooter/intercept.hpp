// WHERE A SHOT MEETS A MOVING PLAYER: the aim point a projectile is fired at so it arrives where
// its target will be, and the pitch a lobbed grenade leaves at to come down there.
//
// - THE TARGET (`MovingTarget`): a place and a velocity. On the ground he goes on in a straight
// line
//   across; in the AIR (`airborne`: off the floor under him) he follows his own arc under the
//   server's gravity and lands on `floor_z`, where he goes on across: a player knocked up by a
//   blast comes down where his arc does, which is where a midair shot is fired.
// - THE CURVE (`MovingTarget::turn`, `leadarc`): a hopper bends his way 100-200 degrees a second;
//   given his turn rate, his way across follows the arc (at most `kArcMost` degrees of it).
// - THE WALLS (`MovingTarget::slide`, `leadwall`): a player cannot run on through a wall; given
//   the caller's world, his way across is slid along what it meets, as the move slides him.
// - THE INTERCEPT (`intercept`): the shot's flight time is from the muzzle to where the target
//   WILL be, not to where he is: the time and the place are worked out again from each other
//   `iterations` times (two or three settle it; one is the plain distance over the speed). A
//   target running at the shooter shortens the flight, one running away lengthens it.
// - THE LOB (`lob_pitch`): the classic grenade leaves at `speed` along the view and `up` along
//   the view's up, and falls at `gravity`; the pitch whose low arc passes through the point is
//   found by halving (the arc's height at the point's distance rises with the pitch up to the
//   longest throw), its flight time with it. A point past the longest throw takes the longest.
//
// Angles in degrees with the protocol's pitch (down positive); the transcendentals are core's
// libm, so the same inputs give the same aim on every machine.
//
// ge:summary Where a shot meets a moving player: intercept by flight time, a target's own arc,
// ge:summary the grenade's lob
// ge:keywords intercept lead flight-time projectile rocket grenade lob arc gravity midair airborne
// ge:keywords ballistic prediction distance aim-point lob_pitch mover_at
// ge:skill shooter bots
// ge:test tests/shooter_intercept_test.cpp
#pragma once

#include "shooter/vec.hpp"

namespace ge::shooter {

inline constexpr float kServerGravity = 800;  // units/s/s
inline constexpr double kArcMost = 90;        // a target's curve followed this far at most, deg

// `leadwall`: the place a player moving from `from` toward `to` across reaches, slid along the
// walls on the way (the caller's world; `to`'s height is only carried).
using SlideAcross = Vec3f (*)(const void* ctx, Vec3f from, Vec3f to);

struct MovingTarget {
  Vec3f at, velocity;
  bool airborne = false;  // on his own arc until `floor_z`
  float floor_z = -1e9f;  // the height his origin lands at
  float gravity = kServerGravity;
  // His way across turning this fast, degrees a second (+ counter-clockwise from above), at most
  // `kArcMost` degrees in all (`leadarc`: a hopper's curve); 0: a straight line.
  float turn = 0;
  // His way across slid along the walls it meets (null: on in a straight line, through them).
  SlideAcross slide = nullptr;
  const void* slide_ctx = nullptr;
};

// Where `m` is `t` seconds on.
Vec3f mover_at(const MovingTarget& m, double t);

struct Shot {
  float speed = 1000;  // along the view, units/s
  bool lob = false;    // a grenade's throw: `up` along the view's up, and the gravity's fall
  float up = 200;
  float gravity = kServerGravity;
};

// The low arc's pitch (protocol degrees, down positive) and flight time from `from` to `to`;
// false when the point is past the longest throw (the longest throw's pitch and time then).
bool lob_pitch(Vec3f from, Vec3f to, const Shot& s, float& pitch, double& flight_s);

struct Intercept {
  Vec3f at;             // where the target will be when the shot gets there
  double flight_s = 0;  // the shot's flight
  float pitch = 0;      // a lob's pitch (down positive)
  bool lobbed = false;  // `pitch` set (a lob)
};

// The shot from `from` at `m`, the target taken `head_s` on before it flies (the reaction, the
// network), the flight worked out `iterations` times (at least 1).
Intercept intercept(Vec3f from, const MovingTarget& m, double head_s, const Shot& s,
                    int iterations);

}  // namespace ge::shooter
