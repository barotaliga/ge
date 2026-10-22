// R42 — THE SKI ROUTES: the ORANGE runs, the grade past black. A black is the steepest a piste
// machine works (R8's `track::max_grade`, 78 %, 38°); past it there is only the mountain as it
// lies. Where a ski area has a line down that country worth skiing it MARKS it (orange stakes
// down a corridor, an orange sign at its head) and calls it a SKI ROUTE: never groomed, never
// pressed, never graded, and harder than any black the area has.
//
// WHERE ONE GOES is FOUND, never sculpted. The face is read as a grid of `route::cell` m squares,
// each one a skier may stand on unless it is a CLIFF (its fall line past `route::cliff`), under a
// LIFT's rope, outside the map's margin or (on the straight line) in the TREES, and a skier goes
// from one to a neighbour (any of sixteen bearings) only downhill and never down a cliff's
// pitch. Two cheapest-path searches over it, FROM THE TOPS (every chair's and gondola's top
// station's rim) and TO THE RUNS (read backwards from every run's corridor), meet at ONE STEEP
// SQUARE whose fall line falls past a black's ceiling and no more than `route::steepest`; the
// joined line is smoothed, laid at a station every few metres and kept only where it is steeper
// than any black, never a cliff, open, not a piste's shoulder, and long and deep enough. Each top
// keeps its best line (the most vertical fallen at a red's pitch or steeper first); the map keeps
// up to `route::count.max`, each off a top of its own and clear of the others.
//
// THE WILD LINE (v10) is drawn, not shortest: a step costs a share of its length by what it
// crosses (the woods, an edge, a cliff's face), dearer straight down the fall line and dearer for
// every metre of trunk it would fell, so it traverses the face, turns through the woods and runs
// along the edges; it may DROP off a crag in a step of two squares, and the line is threaded
// between the trunks a few metres either side and narrowed in the woods. What is then laid down it
// (the trunks felled, the jumps) is route_features.hpp's.
//
// Nothing here moves the ground or draws from a stream: the search is a pure function of the
// built ski area.
//
// ge:summary R42 the ski routes: the orange lines off the tops, straight (v9) or wild (v10)
// ge:keywords ski-route orange-route wild-route route-search steep-square drop route-threading
// ge:keywords lay-ski-routes steepest-along r42
// ge:skill mountain
// ge:test tests/mountain_ski_routes_test.cpp
#pragma once

#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// What the search reads of a built ski area.
struct RouteGround {
  const ge::core::Heightfield& ground;
  std::span<const Run> runs;
  std::span<const Lift> lifts;
  std::span<const TreeDef> trees;
  std::span<const Cliff> cliffs;  // the mountain's cliffs (R22): a wild line skirts and drops them
  double size;
};

// How far either side of a drop's top a route's pitch is excused, m.
inline constexpr double kDropSpan = 24;

// The steepest window of `span` m along a line, m per m, leaving out every window within
// `kDropSpan` of one of `drops`.
double steepest_along(std::span<const TrackPoint> points, double span,
                      std::span<const SkiRoute::Drop> drops = {});

// R42 — the ski routes of a built ski area, `SR1…`: the shortest lines, or the WILD ones.
std::vector<SkiRoute> lay_ski_routes(const RouteGround& area, bool wild);

}  // namespace ge::mountain::mapgen
