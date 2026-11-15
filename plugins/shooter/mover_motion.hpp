// A MOVER'S MOTION, frame by frame: how a door, a plat, a button or a train gets from where it
// stands to where it is going on a server that steps its movers on a fixed frame, for any shooter
// game whose rules move their own brush entities (Shooter's Edge's sequel today; a later game's
// rules the same way). The game keeps the state machine around it (what arriving does, the sounds,
// the push of riders); this is the motion alone, as each frame's velocity.
//
// - **The plain move** (`speed` alone, or `accel` and `decel` equal to it): whole frames at the
//   speed over the travel time, then a last frame covering what is left, so the move ends on its
//   place to the bit.
// - **The accelerated move** (`accel` and `decel` differing from the speed, per frame): from a
//   standing start up to the speed (a lower top when the way is too short to reach it), then down
//   over the span it takes to stop, the last push whatever is left.
// - **The turn**: whole frames at the angle over the travel time, then a last frame turning by
//   what is left, measured from where it stands, so a turn ends on its angles.
//
// The calls are templates over the entity: any struct with the fields named below (the mover's
// own numbers and its motion's state) moves the same. Every float is the caller's IEEE 754 single
// precision, in the order written; held bit-exact by the sequel's lab digests.
//
// ge:summary A mover's motion frame by frame: the plain move, the accelerated move, the turn
// ge:keywords mover movers door plat button train accelerate decelerate turn motion frame
// ge:keywords mover-kit begin_move begin_turn step_motion
// ge:skill shooter
// ge:test tests/shooter_mover_motion_test.cpp
#pragma once

#include <algorithm>

#include "shooter/vec.hpp"

namespace ge::shooter {

// The fixed frame movers step on: its length (s) and its rate (1/s).
struct MoverFrame {
  float step = 0.1f;
  float rate = 10;
};

// What a mover's frame came to.
enum class MotionStep : unsigned char { moving, arrived };

// Sets mover `e` moving from where it stands (`e.origin`) to `dest`.
template <typename M>
void begin_move(M& e, Vec3f dest, const MoverFrame& f) {
  e.dest = dest;
  Vec3f way = dest - e.origin;
  const float distance = normalize(way);
  e.dir = way;
  e.last = false;
  e.velocity = {};
  e.accelerated = !(e.speed == e.accel && e.speed == e.decel);
  if (!e.accelerated) {
    const float time = distance / e.speed;
    e.frames = static_cast<int>(time / f.step);
    e.rest = distance - float(e.frames) * e.speed * f.step;
    if (e.frames == 0) {
      e.velocity = way * (e.rest / f.step);
      e.last = true;
    } else {
      e.velocity = way * e.speed;
    }
    if (distance == 0) e.velocity = {};
    return;
  }
  // The accelerated move: planned on the next frame's step, from a standing start.
  e.rest = distance;
  e.now_speed = 0;
  e.next_speed = 0;
  e.top = 0;
}

// Sets turning mover `e` turning from its angles (`e.angles`) to `dest`.
template <typename M>
void begin_turn(M& e, Vec3f dest, const MoverFrame& f) {
  e.adest = dest;
  e.velocity = {};
  e.accelerated = false;
  const Vec3f delta = dest - e.angles;
  const float time = length(delta) / e.speed;
  e.last = false;
  if (time < f.step) {
    e.frames = 0;
    e.avelocity = delta * (1 / f.step);
    e.last = true;
    if (time == 0) e.avelocity = {};
    return;
  }
  e.frames = static_cast<int>(time / f.step);
  e.avelocity = delta * (1 / time);
}

namespace mover_detail {

// The accelerated move's planning: the top speed and the span it slows over.
template <typename M>
void plan(M& e) {
  e.top = e.speed;
  const auto span = [](float v, float rate) { return v * (v / rate + 1) / 2; };
  const float up = span(e.speed, e.accel), down = span(e.speed, e.decel);
  e.decel_span = down;
  if (up + down > e.rest) {  // too short to reach the speed: a lower top
    const float k = (e.accel + e.decel) / (e.accel * e.decel);
    e.top = (root_of(4 + 8 * k * e.rest) - 2) / (2 * k);
    e.decel_span = span(e.top, e.decel);
  }
}

// The accelerated move's step after a push: the next frame's speed.
template <typename M>
void accelerate(M& e, const MoverFrame& f) {
  if (e.top == 0) {
    if (e.rest < e.accel) {  // shorter than one step of speed: one push of all of it
      e.velocity = e.dir * (e.rest / f.step);
      e.last = true;
      return;
    }
    plan(e);
  } else {
    e.rest -= e.now_speed;
  }
  float v = e.now_speed;
  if (e.rest < e.decel_span) {  // slowing
    if (e.next_speed > 0) {
      v = e.next_speed;
      e.next_speed = 0;
    } else if (v > e.decel) {
      v = std::max(v - e.decel, e.decel);
    }
  } else if (v >= e.top) {  // at the top: past the mark within the frame, the next one slower
    if (e.rest - v < e.decel_span)
      e.next_speed = e.top - e.decel * ((e.decel_span - (e.rest - v)) / v);
  } else {  // speeding up
    const float up = std::min(v + e.accel, e.speed);
    if (e.rest - up < e.decel_span) {
      const float before = std::clamp((e.rest - e.decel_span) / up, 0.0f, 1.0f);
      const float top = std::min(up, e.top);
      v = before * (v + top) / 2 + (1 - before) * top;
      e.next_speed = top - e.decel * (1 - before);
    } else {
      v = up;
    }
  }
  if (e.rest <= v) {  // the last push: what is left
    e.velocity = e.dir * (e.rest / f.step);
    e.last = true;
    return;
  }
  e.now_speed = v;
  e.velocity = e.dir * (v * f.rate);
}

}  // namespace mover_detail

// Mover `e`'s step after a frame's push (`turns`: it turns rather than slides): its next frame's
// velocity, or `arrived` (the velocities zero) when the move is over.
template <typename M>
MotionStep step_motion(M& e, bool turns, const MoverFrame& f) {
  if (e.last) {
    e.velocity = {};
    e.avelocity = {};
    e.last = false;
    return MotionStep::arrived;
  }
  if (e.accelerated) {
    mover_detail::accelerate(e, f);
    return MotionStep::moving;
  }
  if (turns) {
    if (--e.frames > 0) return MotionStep::moving;
    e.avelocity = (e.adest - e.angles) * (1 / f.step);
    e.last = true;
    if (e.avelocity.x == 0 && e.avelocity.y == 0 && e.avelocity.z == 0) {
      e.last = false;
      return MotionStep::arrived;
    }
    return MotionStep::moving;
  }
  if (--e.frames > 0) return MotionStep::moving;
  if (e.rest == 0) {
    e.velocity = {};
    return MotionStep::arrived;
  }
  e.velocity = e.dir * (e.rest / f.step);
  e.last = true;
  return MotionStep::moving;
}

}  // namespace ge::shooter
