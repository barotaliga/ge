// WHAT A MOVING CHARACTER ASKS OF THE WORLD: one question, a box swept along a segment (where it
// first touches something solid, and that surface's normal). The mover knows nothing else about
// the world: a brush map's clip hull, a heightfield, a list of boxes or a game's own colliders
// all answer through `World::trace`, a plain function pointer over the game's context (no
// virtual, nothing allocated). `box_world.hpp` is one answer, for labs, tests and prototypes.
//
// THE CONTRACT a trace keeps, which the mover's slide, step and ground checks lean on:
//   - the box is axis-aligned, `mins`..`maxs` about the moving point (the feet: mins.y = 0);
//   - `fraction` is the share of the segment travelled before touching (1: nothing touched),
//     `end` the point reached, pulled back from the surface by a small skin so the next trace
//     from `end` starts outside it;
//   - `normal` is the touched surface's outward unit normal (undefined when nothing touched);
//   - `start_solid` when the box already overlaps something at `from`, with `all_solid` when it
//     never gets out along the segment (then `fraction` is 0 and `end` is `from`).
//
// ge:summary The world a mover sweeps its hull through: a trace callback (box, from, to -> hit)
// ge:keywords move movement world trace sweep collision hull box callback fraction normal
// ge:keywords start-solid all-solid contact
// ge:skill move
// ge:test tests/move_box_world_test.cpp
#pragma once

#include "move/vec.hpp"

namespace ge::move {

struct Box {
  Vec3 mins, maxs;  // about the moving point
};

struct Hit {
  double fraction = 1;
  Vec3 end{0, 0, 0};
  Vec3 normal{0, 1, 0};
  bool start_solid = false;
  bool all_solid = false;

  bool touched() const { return fraction < 1; }
};

struct World {
  const void* ctx = nullptr;
  Hit (*trace)(const void* ctx, const Box& box, const Vec3& from, const Vec3& to) = nullptr;

  Hit sweep(const Box& box, const Vec3& from, const Vec3& to) const {
    return trace(ctx, box, from, to);
  }
};

}  // namespace ge::move
