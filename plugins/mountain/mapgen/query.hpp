// THE PISTE, ASKED: "where on the piste is this point nearest?" and "where is the piste this
// far down it?", answered once, here, for the generator, the analysis and (from M5) the
// physics, the bot and the renderer.
//
// The nearest-point question is answered off a spatial hash of the segments (24 m cells),
// built the first time a track is asked and kept on it (`Track::index`). THE PISTE IS OPEN:
// nothing wraps. An arc past the finish is the finish, one before the start line the start.
// The ring walk, the order segments are tried in and the tie rule (an equal distance keeps the
// first found) are fixed, so the hit is the same segment on a tie.
// Held by tests/mountain_generate_test.cpp (the forest and the analysis ask it).
//
// ge:summary Where on the piste a point is: nearest_track_point, its spatial hash, track_point_at
// ge:keywords query spatial-hash dense-grid ring-walk
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The segments (by starting station) whose bounding box touches each 24 m cell, in index
// order, on a dense grid over the cells the piste touches: cell (c, r) holds
// `segs[start[k] .. start[k + 1])`, k = (r - r0) * cols + (c - c0). Every cell outside the box
// is empty, so a ring walk reads only the part of a ring that crosses it.
struct TrackIndex {
  int32_t c0 = 0, r0 = 0;      // the box's first column and row
  int32_t cols = 0, rows = 0;  // its size in cells; 0 for a piste with no segment
  std::vector<int32_t> start;  // cols * rows + 1 offsets into `segs`
  std::vector<int32_t> segs;
};

// WHERE ON THE PISTE IS THIS POINT NEAREST: the segment, the arc, the plan distance to the
// centreline and the signed offset (positive right of travel).
TrackHit nearest_track_point(const Track& track, double x, double z);

// `nearest_track_point`, looking only `within` m of the point: `distance` is Infinity when
// the piste is further off. Writes into `hit` (its `index` is left as it was on a miss).
void nearest_within(const Track& track, double x, double z, double within, TrackHit& hit);

// WHERE IS THE PISTE `s` METRES DOWN IT: the centreline between its two nearest stations, the
// arc clamped to the line.
TrackPoint track_point_at(const Track& track, double s);

}  // namespace ge::mountain::mapgen
