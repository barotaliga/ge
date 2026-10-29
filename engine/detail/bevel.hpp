// BEVELS ON SHARP BRUSH EDGES: a convex solid's exposed edges cut back into a chamfer or a round,
// so a wall's corner, a step's nose and a pillar's arris catch a highlight instead of meeting in
// a line no real stone or timber has. The cut is made by ADDING planes to the solid: each round
// is `segments` planes tangent to a circle of `radius_m` tucked into the edge, and the solid's
// faces are found again by the clip (`polygons_of`), so where three bevels meet at a corner the
// corner's facets come out of the same clip.
//
// WHICH EDGES. Every edge of a convex solid turns outward; one is bevelled when it turns by at
// least `min_turn_deg` (a shallow fold stays as it is) and less than `max_turn_deg` (a knife edge
// would be eaten), when both its faces are deep enough to take the set-back (`max_share`), and
// when it is EXPOSED: a neighbour solid flush against either face, or filling the space round the
// edge, means the edge is inside the level's union (two blocks of one wall, a step on a step), and
// a cut there would open a groove. The caller says what is solid with `SolidAt`; `bevel_all`
// bevels a level of solids each against all the others.
//
// The set-back along each face is `radius_m * tan(turn / 2)` (the radius at a right angle),
// shrunk to `max_share` of the shallower face; an edge that would shrink below a quarter of the
// radius is left sharp as `thin`. Deterministic: libm for the angles, exact arithmetic otherwise.
//
// ge:summary Bevels on a convex solid's exposed sharp edges: chamfers and rounds as added planes
// ge:keywords detail bevel chamfer round fillet edge arris brush solid exposed buried bevel_all
// ge:keywords solid-index edge-verdict set-back segments
// ge:skill detail
// ge:test tests/detail_bevel_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "detail/solid.hpp"

namespace ge::detail {

struct BevelSpec {
  double radius_m = 0.03;     // the round's radius
  int segments = 2;           // 1: a chamfer; more: a round of that many facets (at most 8)
  double min_turn_deg = 25;   // an edge turning less stays sharp
  double max_turn_deg = 165;  // a knife edge stays sharp
  double max_share = 0.3;     // the set-back is at most this share of either face's depth
};

// What became of one edge.
enum class EdgeVerdict : uint8_t {
  bevelled,
  flat,    // turns less than min_turn_deg
  knife,   // turns more than max_turn_deg
  thin,    // a face too narrow for a quarter of the radius
  buried,  // a neighbour solid against a face or round the edge
};
std::string_view verdict_name(EdgeVerdict v);

struct BevelEdge {
  int plane_a = 0, plane_b = 0;  // the input solid's planes that meet at it
  Vec3 p0{0, 0, 0}, p1{0, 0, 0};
  double turn_rad = 0;
  double radius_m = 0;  // the radius it was cut with (0 when not bevelled)
  EdgeVerdict verdict = EdgeVerdict::flat;
};

struct BevelResult {
  Solid solid;  // the input's planes first, in order, then every bevel's
  std::vector<BevelEdge> edges;
};

// True when `p` is inside a solid other than the one being bevelled.
using SolidAt = bool (*)(const void* ctx, const Vec3& p);

// Every edge of `s` (both its planes, its ends), each once, in the order the faces list them.
std::vector<BevelEdge> edges_of(const Solid& s, std::span<const Polygon> polys);

// `s` with its exposed sharp edges bevelled; `at` (with `ctx`) says what else is solid, null for
// a solid standing alone.
BevelResult bevel(const Solid& s, const BevelSpec& spec, SolidAt at = nullptr,
                  const void* ctx = nullptr);

// A FAST "WHAT ELSE IS SOLID": the solids' boxes in a grid of `cell_m` cells, built once.
struct SolidIndex {
  std::span<const Solid> solids;
  std::vector<Vec3> lo, hi;  // each solid's box
  double cell_m = 2;
  int64_t nx = 0, ny = 0, nz = 0;
  Vec3 origin{0, 0, 0};
  std::vector<uint32_t> start;  // cell c's solids: ids[start[c] .. start[c + 1])
  std::vector<uint32_t> ids;
};
SolidIndex index_solids(std::span<const Solid> solids, double cell_m = 2);
// The first solid other than `skip` holding `p` by more than `margin`, or -1.
int solid_at(const SolidIndex& index, const Vec3& p, int skip, double margin);

// Every solid of a level bevelled against all the others.
std::vector<BevelResult> bevel_all(std::span<const Solid> solids, const BevelSpec& spec);

}  // namespace ge::detail
