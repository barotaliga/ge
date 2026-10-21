// THE PISTE, ASKED: "where on the piste is this point nearest?" and "where is the piste this
// far down it?", answered once, here, for the generator, the analysis and (from M5) the
// physics, the bot and the renderer. Port of game5 `engine/mapgen/query.ts`
// (`nearestTrackPoint`, `nearestWithin`, `trackPointAt`).
//
// The nearest-point question is answered off a spatial hash of the segments (24 m cells),
// built the first time a track is asked and kept on it (`Track::index`). THE PISTE IS OPEN:
// nothing wraps. An arc past the finish is the finish, one before the start line the start.
// The ring walk, the order segments are tried in and the tie rule (an equal distance keeps the
// first found) are the oracle's, so the hit is the same segment on a tie.
// Parity: through tests/mapgen_generate_test.cpp (the forest and the analysis ask it).
//
// ge:summary Where on the piste a point is: nearestTrackPoint, its spatial hash, trackPointAt
// ge:keywords query nearestTrackPoint nearestWithin trackPointAt spatial-hash
// ge:oracle game5/engine/mapgen/query.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "mapgen/types.hpp"

namespace fall_line::mapgen {

// The segments (by starting station) whose bounding box touches each hash cell, in index
// order. Looked up by key only, never iterated, so its order cannot reach a result.
struct TrackIndex {
  std::unordered_map<int64_t, std::vector<int32_t>> cells;
};

// WHERE ON THE PISTE IS THIS POINT NEAREST: the segment, the arc, the plan distance to the
// centreline and the signed offset (positive right of travel).
TrackHit nearest_track_point(const Track& track, double x, double z);

// `nearest_track_point`, looking only `within` m of the point: `distance` is Infinity when
// the piste is further off. Writes into `hit` (its `index` is left as it was on a miss, as the
// oracle's reused object is).
void nearest_within(const Track& track, double x, double z, double within, TrackHit& hit);

// WHERE IS THE PISTE `s` METRES DOWN IT: the centreline between its two nearest stations, the
// arc clamped to the line.
TrackPoint track_point_at(const Track& track, double s);

}  // namespace fall_line::mapgen
