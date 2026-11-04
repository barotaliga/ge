// The plan's shapes the `real-hints` lab bakes off the open map data: a way's points on a real
// face's map, the line thinned to the bends that keep it near its real one, the longest run of it
// on the map, a ring's inside, its area, its clip to the map's square and its thinning, and ways'
// node lists joined into rings. Every length is in metres on the face's map (real_face.hpp).
//
// ge:summary Lab tooling: lines and rings on a real face's map, thinned, clipped and joined
// ge:keywords osm-shapes douglas-peucker thin clip-to-map join-rings signed-area point-in-ring
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/labs/osm_data.hpp"
#include "mountain/mapgen/real_face_crops.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::labs {

using Pt = mapgen::PlanPoint;
using Ring = std::vector<Pt>;

bool on_map(const Pt& p);
double length_of(const std::vector<Pt>& pts);
// A line thinned to the bends that keep it within `tol` m (Douglas–Peucker).
std::vector<Pt> thin(const std::vector<Pt>& pts, double tol);
// The longest run of a line's points that stay on the map.
std::vector<Pt> longest_on_map(const std::vector<Pt>& pts);
bool inside(const Ring& ring, double x, double z);
// The area a ring holds, m² (its sign its winding).
double signed_area(const Ring& ring);
// The ring clipped to the map's square (Sutherland–Hodgman, one edge of the square at a time).
Ring clip_to_map(const Ring& ring);
// A closed ring thinned (Douglas–Peucker on its two halves, split at the point farthest from its
// first), unclosed.
Ring thin_ring(const Ring& ring, double tol);
// Ways' node lists joined end to end into closed rings; a chain that never closes is dropped.
std::vector<std::vector<int64_t>> join_rings(std::vector<std::vector<int64_t>> parts);
// The way's points on the map, the nodes the data has.
std::vector<Pt> points_of(const mapgen::FaceCrop& f, const OsmData& d, const OsmWay& w);

}  // namespace ge::mountain::labs
