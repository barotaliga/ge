// CURVES IN A BRUSH LEVEL: the runs of flat faces that stand for a round surface (an arch's
// soffit, a vault, a round pillar, a curved wall), found so they can be drawn round. A level made
// of convex brushes can only facet a curve; the facets meet edge to edge along one direction (the
// curve's AXIS) at shallow creases, all bending the same way. That is what is looked for:
//
//   - the visible faces (not hidden) of solid brushes, each as its own polygon;
//   - pairs with edges on one line, overlapping or end to end (staggered slices), linked when their
//   normals are a shallow crease apart
//     (`min_crease_deg` to `max_crease_deg`, 3 and 50 by default) or lie in one plane (a facet cut
//     in two by the level's cells);
//   - a run is the faces linked through creases along one axis (every crease's edge parallel to
//     it, every face's normal square to it) and bending one way: CONVEX (the solid inside the
//     bend: a pillar) or CONCAVE (the open side inside: an arch's soffit);
//   - in a run, a facet of a curve is as wide as the curve's radius and the turns at its two
//     creases make it (r (tan a/2 + tan b/2)); patches whose width implies a radius more than
//     `width_spread` off the run's median are dropped (a flat wall a chamfer turns off, the long
//     straight a vault springs from);
//   - the rest is kept when it turns through at least `min_facets` distinct normals.
//
// Each run is fitted with the circle its faces are tangent to (least squares over the facets'
// planes, in the plane square to the axis): the centre, the radius to the facets' planes, and the
// sweep. A run whose planes do not all lie within `fit_tolerance` of that radius, or whose radius
// is outside `min_radius_m` to `max_radius_m`, is not a curve.
//
// ge:summary Finds the runs of faces that facet an arch, a vault or a round pillar, and fits them
// ge:keywords curve curves arch vault pillar column round smooth facet crease axis radius fit
// ge:keywords concave convex soffit detect classic clone
// ge:skill rooms
// ge:test tests/rooms_curves_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "rooms/brush_build.hpp"
#include "rooms/level.hpp"

namespace ge::rooms {

struct CurveOptions {
  double min_crease_deg = 3;
  double max_crease_deg = 50;
  size_t min_facets = 3;        // distinct normals a run turns through
  double min_edge_m = 0.05;     // the shortest edge that can link two faces
  double line_tol_m = 0.02;     // edges this near one another's line lie on one line
  double join_gap_m = 0.02;     // edges on one line link when they overlap or this nearly meet
  double width_spread = 1.6;    // a facet's width may imply a radius this factor off the run's
  double fit_tolerance = 0.15;  // each facet's plane within this share of the radius of it
  double min_radius_m = 0.1;
  double max_radius_m = 12;
};

struct Curve {
  double axis[3] = {0, 1, 0};    // unit: the direction the curve runs along
  double centre[3] = {0, 0, 0};  // a point on the axis, m (at the run's middle along it)
  double radius = 0;             // m, from the axis to the facets' planes
  double sweep_deg = 0;          // how far round the facets' normals turn
  double length = 0;             // m, the run's extent along the axis
  bool concave = false;          // the open side is inside the bend (an arch), else a pillar
  size_t facets = 0;             // distinct normals
  std::vector<uint32_t> faces;   // into Level::faces, ascending
};

// Every run of faces in `level` that facets a curve, in the order of their lowest face.
std::vector<Curve> find_curves(const Level& level, const CurveOptions& options = {});

// `mesh` (the level's `build_brushes`) with every curve's faces drawn round (round.cpp): each
// triangle cut where the curve turns through a multiple of `segment_deg`, every corner moved onto
// the cylinder through the facets' corners (so a pillar still covers the hole the floor leaves for
// it) and shaded with the cylinder's normal. The faces a body meets are not touched: collision
// stays on the facets.
BrushMesh round_curves(const Level& level, const BrushMesh& mesh, std::span<const Curve> curves,
                       double segment_deg = 7.5);

}  // namespace ge::rooms
