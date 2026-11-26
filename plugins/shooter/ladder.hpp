// A LADDER IN A VERSION 38 MAP: a brush whose contents carry `kSqLadder` is climbed by any
// player's move that meets it, by one rule both moves share (the classic move in pmove.cpp and
// Quake II's in plugins/classic/q2_move.cpp), so a map of that family plays the same whichever
// move runs on it. A map of the hull family has no such bit, so a move there never climbs.
//
// - **Taking hold** (`ladder_ahead`): a trace of the player's box one unit ahead along the view's
//   level direction stops on a brush with the ladder bit. Checked once a command, after the
//   ground is found, and never while a water jump's clock runs.
// - **The wish on the rungs** (`ladder_wish`): while the speed up or down is at most 200, the
//   wish's height is 200 up, 200 down or nothing, and its level part is held to 25 each way
//   (x and y apart). Looking 15 degrees or more up or down while moving forward climbs that way;
//   else the up move does (jump climbs, crouch descends).
// - **Holding on** (`ladder_hold`): with no wish up or down, gravity takes the speed up or down
//   toward 0 and stops there, so a player on the rungs neither falls nor rises.
// - **Friction**: on the rungs the ground's friction applies as if standing, and the water's
//   does not (each move applies it in its own friction step).
//
// Pure and allocation-free; floats in the order written.
//
// ge:summary The ladder rule both classic moves share: taking hold, the wish, holding on
// ge:keywords ladder climb rungs ladder-ahead ladder-wish ladder-hold contents-ladder version-38
// ge:keywords remix-ladder
// ge:skill shooter move
// ge:test tests/shooter_ladder_test.cpp
#pragma once

#include <cstdint>

#include "shooter/brush_contents.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter {

inline constexpr float kLadderSpeed = 200;  // u/s: climbing and descending
inline constexpr float kLadderSide = 25;    // u/s: the level wish's limit, each axis apart
inline constexpr float kLadderLook = 15;    // degrees up or down that steer the climb
inline constexpr float kLadderReach = 1;    // u: how far ahead of the box a ladder is found

// The point one unit ahead along the view's level direction, where `ladder_ahead` traces to.
inline Vec3f ladder_probe_end(Vec3f origin, Vec3f forward) {
  Vec3f flat{forward.x, forward.y, 0};
  normalize(flat);
  return {origin.x + flat.x * kLadderReach, origin.y + flat.y * kLadderReach,
          origin.z + flat.z * kLadderReach};
}

// Whether a trace toward `ladder_probe_end` that stopped at `fraction` on a brush with contents
// `bits` takes hold of a ladder.
inline bool ladder_ahead(float fraction, int32_t bits) {
  return fraction < 1 && (bits & kSqLadder) != 0;
}

// The wish on the rungs: `pitch` the view's (down positive), `forward` and `up` the command's.
// Leaves the wish alone while the speed up or down passes 200.
inline void ladder_wish(Vec3f& wish, float vertical_speed, float pitch, int forward, int up) {
  const float vz = vertical_speed < 0 ? -vertical_speed : vertical_speed;
  if (vz > kLadderSpeed) return;
  const bool looks = (pitch <= -kLadderLook || pitch >= kLadderLook) && forward > 0;
  const bool climbs = looks ? pitch <= -kLadderLook : up > 0;
  const bool descends = looks ? pitch >= kLadderLook : up < 0;
  wish.z = climbs ? kLadderSpeed : descends ? -kLadderSpeed : 0.0f;
  for (int i = 0; i < 2; ++i) {
    if (wish[i] < -kLadderSide) wish[i] = -kLadderSide;
    if (wish[i] > kLadderSide) wish[i] = kLadderSide;
  }
}

// Holding on with no wish up or down: gravity (`gravity` u/s², over `seconds`) takes the speed
// up or down toward 0, never past it.
inline void ladder_hold(Vec3f& velocity, float gravity, float seconds) {
  if (velocity.z > 0) {
    velocity.z -= gravity * seconds;
    if (velocity.z < 0) velocity.z = 0;
  } else {
    velocity.z += gravity * seconds;
    if (velocity.z > 0) velocity.z = 0;
  }
}

}  // namespace ge::shooter
