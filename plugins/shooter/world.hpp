// THE SOLIDS OF ONE MOVE and the three questions the move asks of them: where does the player's
// box stop (`player_trace`), is the box free at a point (`box_free`), and what is at a point
// (`water_contents`, `brush_contents`).
//
// A move's list holds at most `kMaxSolids`: index 0 is the world, then the solid entities near
// the player (brush models such as doors, boxes, other players' boxes) in the order the server
// listed them; choosing them is the server's (M18o2). A brush model is traced through its
// player clip hull (hull 1) offset by its origin; a box is grown by the player's box (its mins
// minus the player's maxs, its maxs minus the player's mins) into a six-plane hull.
//
// `player_trace` traces each solid in turn, skipping (all but the world) those whose grown
// bounds miss the swept segment's bounds padded by 1; a trace that started in a solid counts as
// fraction 0; the smallest fraction wins, ties keeping the earlier solid, and the winner's
// index is the trace's `ent` (-1 when nothing was hit). Pure and allocation-free.
//
// A third kind, `brushes`, is a model of a map of the version 38 family traced through its
// brushes (`ge::brush::BrushTracer`) rather than a clip hull: the same player box, colliding with
// what a living player's box does (`kSqMaskPlayerSolid` but monsters' bodies, which the list
// holds as boxes), and its contents bits read back as the hull family's numbers (solid, lava,
// slime, water, empty). So the move, the nav and the bots ask one list whichever family the map
// is (M18v2c). The tracer is the caller's and outlives the list; a trace through it allocates
// nothing.
//
// ge:summary The solids of one move: the player box trace, the free-box test, point contents
// ge:keywords physent solid solids world brush-model box player_trace box_free water_contents
// ge:keywords brush_contents hull1 offset bounds touch brushes version-38 one-trace point_trace
// ge:skill shooter
// ge:test tests/shooter_world_test.cpp
#pragma once

#include <cstdint>

#include "shooter/hull.hpp"

namespace ge::brush {
class BrushTracer;
}  // namespace ge::brush

namespace ge::shooter {

constexpr int kMaxSolids = 64;

enum class SolidKind : uint8_t { brush, box, brushes };

struct Solid {
  SolidKind kind = SolidKind::brush;
  Vec3f origin;
  Vec3f mins, maxs;  // a box's extent about its origin; a brush model's bounds
  Hull point_hull;   // a brush model's hull 0
  Hull clip_hull;    // a brush model's hull 1 (the player's box)
  ge::brush::BrushTracer* tracer = nullptr;  // `brushes`: the map's tracer
  uint32_t model = 0;                        // `brushes`: the map's model (0 the world)
};

struct MoveWorld {
  Solid solids[kMaxSolids];
  int count = 0;  // solids[0] is the world
};

// Sweeps the player's box from `start` to `end` through every solid of the list.
HullTrace player_trace(const MoveWorld& world, Vec3f start, Vec3f end);
// Whether the player's box at `p` is outside every solid of the list.
bool box_free(const MoveWorld& world, Vec3f p);
// The world's point hull at `p` (the water level's three samples read it alone).
int water_contents(const MoveWorld& world, Vec3f p);
// Every brush model's point hull at `p`: solid if any is solid, else the first answer that is
// not empty, else empty (the water jump's two samples).
int brush_contents(const MoveWorld& world, Vec3f p);
// A point (a line, no box) traced through the world alone: sight and the floor under a point.
HullTrace point_trace(const MoveWorld& world, Vec3f start, Vec3f end);
// The world solid of a version 38 map: its model 0 traced by `tracer`, bounds from `mins`/`maxs`.
Solid brushes_solid(ge::brush::BrushTracer& tracer, uint32_t model, Vec3f origin, Vec3f mins,
                    Vec3f maxs);

}  // namespace ge::shooter
