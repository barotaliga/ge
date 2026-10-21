// R24 — THE DROPS: cliff bands a black piste is built over.
//
// A drop is R22's cliff profile laid ACROSS the line, added to the graded line by arc before
// the corridor is pressed, with no apron (the face falls straight back onto the line, so the
// fall is the drop's and no more). Laid on a stretch that falls steeply and turns little, off
// the start, the finish and every gate, and before the kickers, which keep clear of them.
// Dealt off a stream of their own, so a grade without drops (every v1 map) draws nothing.
// Held by tests/mountain_generate_test.cpp; no v1 map lays one, so the black's drops are
// held from M4b on.
//
// ge:summary R24 drops across a black piste: chosen on the graded line and published as cliffs
// ge:keywords drop drops
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/track.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// A drop as it is laid: the station of its edge and its profile, m.
struct TrackDrop {
  size_t index;
  double drop;
  double shelf;
  double face;
  double apron;
  double landing;
};

// R24 — choose the drops across the piste and add them to the graded profile, in order down
// the line, none over a `keep_off` stretch.
std::vector<TrackDrop> lay_drops(uint32_t sub, Track& track, const GradeRow& grade,
                                 std::span<const ArcSpan> keep_off = {});

// Publish the drops against the finished line: `D1…`, each a cliff across the piste.
std::vector<Cliff> publish_drops(const Track& track, const std::vector<TrackDrop>& drops);

}  // namespace ge::mountain::mapgen
