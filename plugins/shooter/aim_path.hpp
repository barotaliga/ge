// A PLAYER'S MOTION READ OFF THE PATH HE HAS JUST RUN: what a shot leading him (the aim help's
// `aim_lead`, a bot's `intercept`) takes for where he goes next. One velocity and whether he is on
// his own arc in the air, from his places over the last moments, by one of several rules
// (`PathGuess`); which rule leads a missile onto real players best is measured on recorded
// matches by the `se-aimlead` lab, not guessed.
//
// THE RULES:
// - `still`: he stays where he is (no lead at all; the yardstick).
// - `last`: the velocity of his last step (the server's own, near enough).
// - `mean`: his mean velocity over the last `window` seconds, every axis.
// - `mean_flat`: the mean over `window` across the floor; up and down his last step's (an arc
//   in the air keeps its true rise or fall).
// - `fading`: his last step's velocity, his run across fading with the time constant `window`
//   (a player soon turns or stops, so the farther ahead the less of his run is trusted).
// - `turning`: his last step's velocity, turning across the floor at the rate his heading turned
//   over the last `window` seconds (a strafe-jumper's curve).
// - `smoothed`: his velocity smoothed step by step, each step's pulling it by 1 - e^(-dt/window)
//   (between the last step's twitch and a long mean's lag).
// - `speeding`: his last step's velocity and his change of it across over the last `window`
//   seconds, kept: v t + a t^2 / 2, never faster across than `kPathTopSpeed` on average.
// - `sway_centre`: his course (the mean over `window`) from the middle of his sway rather than from
//   where he is: a strafer weaving left and right is aimed at the line he weaves about.
//
// A RUNNING THRESHOLD (`run`): below it across, his run is read as none, whatever the rule (a
// player stepping about an item is not going anywhere).
//
// IN THE AIR: his last step rises or falls (more than `kPathStillZ` units a second) or he is above
// the floor under him; the floor is the lowest his origin stood over the last `kPathFloorS`
// seconds (no map is read, so a lift reads as the air).
// THE ARC (`arc`): a stricter read of the air for any rule. He is in the air only when his last
// steps fall by the game's gravity (`kPathGravity`, within `kPathArcSlack`) or he has just left the
// ground upward (a jump); his rise or fall is then his last step's and the floor he lands on is the
// height he took off from (the arc's start found by walking back while the steps still fit it).
// On the ground he stays at his height: stairs and ramps never read as a fall.
//
// ge:summary A player's motion read off his recent path: still, last step, mean over a window
// ge:keywords aim-path path-guess lead prediction velocity window mean airborne floor aim_lead
// ge:keywords intercept read_path fading turning turn-rate
// ge:skill shooter
// ge:test tests/shooter_aim_path_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "shooter/vec.hpp"

namespace ge::shooter {

inline constexpr float kPathStillZ = 20;  // units/s: a rise or fall slower than this is the floor
inline constexpr float kPathFloorS = 1.5f;
// s of path the floor under him is read from
inline constexpr float kPathTopSpeed = 600;  // units/s across: `speeding`'s cap (a fast hopper)
inline constexpr float kPathGravity = 800;   // units/s^2: the fall `arc` looks for
inline constexpr float kPathArcSlack = 300;  // units/s^2: how far a step's fall may stray from it
inline constexpr float kPathJumpZ = 150;     // units/s: a rise this fast off level ground is a jump

struct PathPoint {
  float t = 0;  // s
  Vec3f origin;
};

enum class PathGuess : uint8_t {
  still,
  last,
  mean,
  mean_flat,
  fading,
  turning,
  smoothed,
  speeding,
  sway_centre
};

struct PathRule {
  PathGuess guess = PathGuess::last;
  float window = 0.1f;  // s: `mean`, `mean_flat`, `fading`'s time constant, `turning`
  float run = 0;        // units/s: slower across than this (his last step) reads as standing
  bool level = false;   // no height read: he stays at his height, never on an arc
  bool arc = false;     // the air read by gravity, the floor at his takeoff (above)
};

struct PathMotion {
  Vec3f velocity;          // units/s
  bool airborne = false;   // on his own arc (falls by the gravity)
  float floor_z = -1e30f;  // the origin's height he lands at
  float fade = 0;          // s: `fading`'s time constant (0 none)
  float turn = 0;          // rad/s: `turning`'s rate, anticlockwise from above (0 none)
  Vec3f accel;             // units/s^2 across: `speeding`'s
  Vec3f shift;             // units: where the lead starts from, off his place (`sway_centre`)
};

// His motion now (the last point) by `rule`, from `past` (oldest first, at least one point).
PathMotion read_path(std::span<const PathPoint> past, PathRule rule);

// His place at `t` along `past` (linear between points; held at either end).
Vec3f path_at(std::span<const PathPoint> past, float t);

// The rule's name for tables ("mean 0.20 s").
const char* path_guess_name(PathGuess g);

}  // namespace ge::shooter
