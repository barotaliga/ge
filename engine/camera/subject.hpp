// THE SUBJECT: what a rig reads of the thing it follows, filled by the game from its own state
// once a display frame (interpolated between the last two steps). The camera reads no simulation
// state, so the same boom follows a skier, a sled or a car without a word of any of them.
//
// A first-person game hands its view angles here straight from this frame's input: the look is
// never smoothed or delayed by the camera; the bob, the roll and the kicks are added on top and
// are presentation alone (they never go back to the simulation).
//
// MOMENTS (`Moment`) are this frame's events with a size: a landing (its fall speed), a hit (its
// size and the direction it came from), a shot fired, an explosion near the camera, a teleport
// (a respawn, a reset: the followers snap instead of chasing it).
//
// ge:summary The subject a camera follows: pose, velocity, view angles, ground contact, moments
// ge:keywords camera subject target follow velocity orientation quat heading look-yaw look-pitch
// ge:keywords grounded crouch zoom surface moment event landing hit shot explosion teleport
// ge:keywords add_moment
// ge:skill camera
// ge:test tests/camera_camera_test.cpp
#pragma once

#include <cstdint>

#include "core/quat.hpp"

namespace ge::camera {

using core::Quat;
using core::Vec3;

enum class Moment : uint8_t {
  landing,    // size: the fall speed at touchdown, m/s
  hit,        // size: 0..1; dir: the way the blow travelled (world, unit)
  shot,       // size: 0..1, the weapon's punch
  explosion,  // size: 0..1 trauma at the camera
  teleport,   // the subject was put somewhere: snap, never chase
};

struct Event {
  Moment kind = Moment::landing;
  double size = 0;
  Vec3 dir{0, 0, 0};
};

inline constexpr int kMaxEvents = 8;

struct Subject {
  Vec3 pos{0, 0, 0};    // world metres: the body's origin (its feet)
  Vec3 vel{0, 0, 0};    // m/s
  Quat q{0, 0, 0, 1};   // body to world: x right, y up, z forward
  double heading = 0;   // the way the body faces, rad
  double look_yaw = 0;  // the player's view angles, rad (first person, a free look)
  double look_pitch = 0;
  bool grounded = true;  // standing on something
  double crouch = 0;     // 0 standing .. 1 crouched
  double zoom = 0;       // 0 the rig's own lens .. 1 zoomed (first person)
  double surface = 0;    // 0 smooth .. 1 rough under it (a tremor's share)
  int event_count = 0;
  Event events[kMaxEvents]{};
};

// Add a moment to this frame's subject (dropped past kMaxEvents).
inline void add_moment(Subject& s, Moment kind, double size, Vec3 dir = {0, 0, 0}) {
  if (s.event_count < kMaxEvents) s.events[s.event_count++] = Event{kind, size, dir};
}

}  // namespace ge::camera
