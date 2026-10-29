// CLIP HULLS BUILT FROM CONVEX BRUSHES: the hand-built worlds the move's scenarios and tests run
// on. A brush is a convex solid, a list of planes `normal dist` (a point is inside when it is
// behind every plane) and a contents (solid, water, slime, lava); every brush lists its six axial
// planes as well as any slanted one, so growing it by a box is only moving each plane.
//
// The hulls are a CHAIN, brush by brush and plane by plane in the order written: each plane is
// one node, whose front child is the first node of the next brush in the hull (or empty after
// the last brush) and whose back child is the brush's next plane, or the brush's contents after
// its last plane. So a point takes the contents of the first brush that holds it.
// - Hull 0 (points) holds every brush, its planes as written.
// - A box hull (`kPlayerMins`..`kPlayerMaxs` for hull 1) holds the SOLID brushes only, each plane
//   moved out by the box: `dist - (n.x * ex + n.y * ey + n.z * ez)` with `e` the box's mins on an
//   axis whose normal is positive and its maxs otherwise, summed in float in axis order.
// A map's compiled hulls (M18b) answer the same queries; this builder is how a scenario stands
// up a world without one. `build_brush_model` builds a whole brush model (the world, a door, a
// plat) the way a server traces it: hull 0, hull 1 (the player's box) and hull 2 (the large box),
// each with the box it was grown by (`BrushModel::clip_mins`). Held by
// tests/shooter_brush_hull_test.cpp.
//
// ge:summary Clip hulls built from convex brushes: hull 0 and the player's hull as a plane chain
// ge:keywords brush brushes convex clip-hull hull0 hull1 chain expand grow player-box build
// ge:keywords build_hull brush_contents brush-model build_brush_model hull2 large-box clip-mins
// ge:skill shooter
// ge:test tests/shooter_brush_hull_test.cpp
#pragma once

#include <vector>

#include "shooter/hull.hpp"

namespace ge::shooter {

// The player's box about its origin, units: the origin is 24 above the feet.
constexpr Vec3f kPlayerMins{-16, -16, -24};
constexpr Vec3f kPlayerMaxs{16, 16, 32};
// The large box hull 2 is grown by, units.
constexpr Vec3f kLargeMins{-32, -32, -24};
constexpr Vec3f kLargeMaxs{32, 32, 64};

struct Brush {
  int contents = kContentsSolid;
  std::vector<Plane> planes;  // type is filled by build_hull
};

// A built hull: owns its arrays; `hull()` is the view the trace reads.
struct HullData {
  std::vector<Plane> planes;
  std::vector<ClipNode> nodes;

  Hull hull() const {
    return {planes.data(), nodes.data(), 0, static_cast<int32_t>(nodes.size()) - 1};
  }
};

// Hull 0: every brush, planes as written. An empty list gives a hull that is all empty.
HullData build_point_hull(const std::vector<Brush>& brushes);
// The hull of a box (mins, maxs) swept through the solid brushes.
HullData build_box_hull(const std::vector<Brush>& brushes, Vec3f mins, Vec3f maxs);

// A brush model as a trace reads it: three hulls (0 points, 1 the player's box, 2 the large
// box), the box each was grown by (`clip_mins`, so a mover's box `mins` is offset by
// `clip_mins[h] - mins`), and the model's bounds. A view: the arrays belong to `ModelData`.
struct BrushModel {
  Hull hulls[3];
  Vec3f clip_mins[3];
  Vec3f mins, maxs;
};

// A brush model's owned hulls. Never copy one once `model()` has been taken (moving is fine).
struct ModelData {
  HullData hulls[3];
  Vec3f mins, maxs;

  BrushModel model() const;
};

// The three hulls of a model made of these brushes, and its bounds (each brush's axial planes).
ModelData build_brush_model(const std::vector<Brush>& brushes);

}  // namespace ge::shooter
