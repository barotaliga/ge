// THE ENGINE MOVER'S WORLD OVER CLIP HULLS: a list of brush models (the world first, then doors,
// plats, boxes), each at its origin, answering `move::World::trace` (engine/move/world.hpp) so
// the engine's mover walks a brush map; the contents at a point; a brush model's liquids laid as
// `engine/water` volumes so the engine's swimmer reads them through `water::sample`.
//
// THE AXES. The engine is y up with its heading 0 = +z, clockwise from above; the map is z up
// with its yaw counter-clockwise from +x. The view's own axes are kept: the map's forward (+x)
// is the engine's +z, its right (-y) the engine's +x and its up (+z) the engine's +y, so
// `to_engine(x, y, z) = (-y, z, x)`, a yaw becomes its negative and a pitch (down positive on
// the map) its negative too. Lengths stay in map units (a row's `unit`, 32 to the metre).
//
// THE TRACE. The engine's box is about the feet; it is answered by the model's hull for its
// size: a point by hull 0, a box no wider than the player's by hull 1 (the player's box), a
// wider one by hull 2, each traced from the point where the box's mins meet the hull's
// `clip_mins` (brush_hull.hpp), so the standing player's box traces hull 1 from the map's
// origin, 24 above the feet. A box of another height than the hull it falls to is answered as
// that hull's box: the classic hulls are the only boxes a brush map stores. Every model is traced
// in turn and the smallest fraction wins, ties keeping the earlier model; the solid reported is
// the model's index + 1 (0: none). The end point is the hull trace's own, 1/32 unit back from
// the plane along the segment (the engine's skin). A trace that starts in a solid is reported
// ALL SOLID with fraction 0 at its start: a hull trace has no exit to slide along, and the mover
// stops the body, as the classic move does.
//
// THE LIQUIDS. `add_liquids` walks a model's hull 0: every leaf whose contents are water, slime
// or lava becomes one convex `water::Hull` (the half-spaces on its path from the first node, cut
// to the model's bounds; paths whose box of bounds empties are dropped), laid as one volume body
// each so a point inside any of them is under that body's surface. Water is `water::Stuff`'s
// default; slime and lava take the engine's rows of those names, every swim scale 1 (a classic
// feel in every liquid), and the caller is told which contents each body was laid for.
//
// ge:summary The engine mover's world over clip hulls: the trace, contents, liquids as volumes
// ge:keywords hull-world brush-model mover-world trace engine-axes to_engine to_map axes y-up
// ge:keywords hull0 hull1 hull2 clip-mins contents_at add_liquids water slime lava
// ge:keywords volume leaf swimmer
// ge:skill shooter move water
// ge:test tests/shooter_hull_world_test.cpp
#pragma once

#include <vector>

#include "move/world.hpp"
#include "shooter/brush_hull.hpp"
#include "water/world.hpp"

namespace ge::shooter {

// The models a mover meets; the arrays belong to the caller and outlive every trace.
struct HullWorld {
  const BrushModel* models = nullptr;  // [0] the world
  const Vec3f* origins = nullptr;      // each model's origin, map units (null: all at 0)
  int count = 0;
};

// The map's axes and the engine's (see the header).
move::Vec3 to_engine(Vec3f map);
Vec3f to_map(const move::Vec3& engine);

// The engine's world answered by `hulls` (kept by pointer: it must outlive the World).
move::World mover_world(const HullWorld& hulls);

// The contents at an engine point: every model's hull 0, solid if any is solid, else the first
// answer that is not empty, else empty.
int contents_at(const HullWorld& hulls, const move::Vec3& engine_point);

// The liquids of `model` (its hull 0, at `origin`) laid in `water` as volumes, in metres
// (`units_per_metre` map units each). Returns how many bodies were laid; `kinds` gets one
// contents number per body laid (kContentsWater, kContentsSlime, kContentsLava).
int add_liquids(const BrushModel& model, Vec3f origin, double units_per_metre, water::World& water,
                std::vector<int>& kinds);

}  // namespace ge::shooter
