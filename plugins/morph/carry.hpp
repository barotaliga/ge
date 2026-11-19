// A MORPH MODEL TAKEN APART AND ANOTHER CARRIED IN ITS HANDS: the pieces a figure is built of
// (vertices joined by triangles, the seam's copies of a position one piece), the figure with some
// pieces cut out, and a second model posed in every one of the figure's frames the way one of its
// pieces moves (a gun held in the hands follows the gun baked into the figure).
//
// - **Pieces** are numbered by size, the largest (the body) 0; two vertices are one piece when a
//   triangle joins them or they are copies of one file vertex (`Model::source_vertex`).
// - **A piece's motion** from one frame to another is the rotation and move that carry its points
//   nearest onto the other frame's (least squares, the unit quaternion of the largest eigenvalue
//   of the 4x4 matrix the cross-covariance makes). The points are the file's, a byte a coordinate
//   apart, so a fit is good to about half a unit.
// - **Carried**: the held model's first frame is placed against the figure's reference frame by a
//   `Rigid` mount, then moved in each frame by its grip piece's motion from that frame; the result
//   has the figure's frames (names, groups, intervals) and the held model's skins and mesh, so it
//   is drawn on the figure's frame number. `carried_by` takes a placement for every frame instead
//   (a weapon put in two hands frame by frame).
// - **Along the surface**: the shortest way over the triangles' edges between vertices of one
//   piece (a seam's copies of a file vertex no distance apart), and a piece's ends: the vertices
//   no neighbour lies farther from a root than (a figure's head, hands and feet from its chest).
//
// ge:summary A figure's pieces cut out and a held model posed by a piece's motion, every frame
// ge:keywords carry pieces connected-parts cut-out keep_vertices rigid-fit fit_rigid horn
// ge:keywords carried-model held-weapon vwep visible-weapons figure morph frames quaternion
// ge:keywords carried_by surface_distance surface_ends geodesic extremities hands dijkstra
// ge:skill morph
// ge:test tests/morph_carry_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "morph/model.hpp"

namespace ge::morph {

// A rotation and a move: p' = r p + t (r row-major), model units. A fit is a pure rotation; a
// mount may scale too (a pickup model drawn smaller in the hands).
struct Rigid {
  double r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  double t[3] = {0, 0, 0};
};
Rigid compose(const Rigid& a, const Rigid& b);  // a after b
void apply(const Rigid& m, const float in[3], float out[3]);

// Each vertex's piece, 0 the largest (ties: the piece holding the lower vertex first).
std::vector<int> pieces(const Model& m);
// The vertices of piece `piece`, in order.
std::vector<uint32_t> piece_vertices(std::span<const int> piece_of, int piece);

// `m` with only the vertices `keep` marks (one flag a vertex); a triangle stays when its three
// corners do. Frames, skins and the rest as they were.
Model keep_vertices(const Model& m, const std::vector<bool>& keep);

// The rigid motion carrying vertices `of` from frame `from` to frame `to` (both indices into
// `m.frames`); the identity for fewer than three vertices or a frame out of range.
Rigid fit_rigid(const Model& m, std::span<const uint32_t> of, size_t from, size_t to);

// `held`'s first frame placed by `mount` into `figure`'s frame `reference`, then posed in each of
// `figure`'s frames by the motion of `grip` from `reference` (see the header).
Model carried(const Model& held, const Model& figure, std::span<const uint32_t> grip,
              size_t reference, const Rigid& mount);

// `held`'s first frame placed into each of `figure`'s frames by `per_frame[f]` (one a frame; a
// missing one is the last given).
Model carried_by(const Model& held, const Model& figure, std::span<const Rigid> per_frame);

// Every vertex's distance from `from` over the triangles' edges in frame `frame`; a vertex the
// surface does not reach is +infinity.
std::vector<double> surface_distance(const Model& m, size_t frame, uint32_t from);
// The ends of the surface `from` reaches in frame `frame`: vertices whose distance from it no
// neighbour exceeds, one a file vertex (the lowest copy), the farthest first.
std::vector<uint32_t> surface_ends(const Model& m, size_t frame, uint32_t from);

}  // namespace ge::morph
