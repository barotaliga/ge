// R5–R8, R10 — THE PISTE: walked, measured, graded into the mountain. Port of game5
// `engine/mapgen/track.ts`.
//
// THE SHAPE IS A WALK DOWN THE FALL LINE: two metres at a time from the start, its heading
// bending either side of the fall line on a slow wave, with a few sweeps held across the face
// and schusses held down it, the untouched ground steering it off any heading it would not
// fall along or would fall along too steeply. The heading never comes within a right angle of
// straight across, so z grows every step: the line is an open DESCENT and cannot cross itself.
// A controller reads the fall left against the length left and widens or narrows the wave, so
// a walk lands in its length band by construction.
//
// THE GRADING (R8) is two envelopes rather than a blur: the line that never climbs (the cut
// from above and the fill from below, and their mean), then the line never steeper than the
// rule the same way, a light blur, and the finish laid flat. `stamp_corridor` then presses the
// finished line into the ground: level across the width (a little camber kept on a traverse),
// the flat shoulder and the bench, the groomer's windrow on that bench (R18), a bank back to
// the mountain, and the packed field (R10).
// Parity: through tests/mapgen_generate_test.cpp (`points`, `ground`, `packed`).
//
// ge:summary R5-R8, R10, R18 the piste: the walk, the grading, the bends and the corridor stamp
// ge:keywords track piste drawPiste gradePiste stampCorridor tightestBend windowGrades
// ge:keywords minSeparation
// ge:oracle game5/engine/mapgen/track.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mapgen/terrain.hpp"
#include "mapgen/types.hpp"

namespace fall_line::mapgen {

// The piste while the generator is still working on it. `track` is what the queries read.
struct Piste {
  Track track;
  std::vector<double> raw;    // the untouched mountain's height under each station, m
  std::vector<double> cross;  // the cross-fall the corridor keeps at each station (R8 camber)
};

// R5 — walk the piste from the start: its line, its length, its width; or why it failed.
std::variant<Piste, std::string> draw_piste(ge::core::Rng& rng, const TerrainPlan& plan,
                                            const ge::core::Heightfield& ground,
                                            const Spawn& start);

// R8 — grade the profile into the mountain: no window steeper than `max_grade` (and, given a
// `colour`, no colour window steeper than it). Writes each station's y and the piste's `raw`;
// returns why a line cannot be graded inside `track.max_cut`.
std::optional<std::string> grade_piste(Piste& piste, const ge::core::Heightfield& country,
                                       double max_grade, std::optional<double> colour);

// Headings off the central difference down the line, one-sided at the ends.
void set_headings(std::vector<TrackPoint>& points);

// R6 — the tightest bend: the circumradius of stations `track.turn_window` apart, m.
double min_radius(const Track& track);

struct BendReading {
  double radius;  // m
  double floor;   // the least radius the pitch there allows (bend_floor), m
  double grade;   // the line's fall there, m per m
  double s;       // where, m
};

// R6 — the bend that comes closest to breaking the rule, `y_at` the height at a station.
BendReading tightest_bend(const Track& track, const std::function<double(size_t)>& y_at);

// R6 — the least room the inside bench has in a bend, m (negative where it folds).
double bend_room(const Track& track);

// R5 — the least plan distance between stations more than `separation.along` apart, m.
double min_separation(const Track& track);

struct WindowGrades {
  double steepest;
  double gentlest;
};

// The fall over every `track.grade_window` of a profile, m per m, skipping windows `skip`
// marks at either end (none when `skip` is empty).
WindowGrades window_grades(const std::vector<double>& y, double step,
                           const std::vector<uint8_t>& skip);

// The arc from which the line is the finish's (the run-out and the straight).
inline double finish_from(double length) {
  return length - rules::track::finish - rules::track::runout;
}

// What the corridor stamp leaves besides the graded ground.
struct Corridor {
  ge::core::Heightfield packed;  // R10: 0 powder … 1 packed, on the ground's grid
  std::vector<int32_t> near;     // per cell, the station starting the nearest segment (-1: none)
  std::vector<float> along;      // per cell, how far along that segment, 0..1
  std::vector<float> dist;       // per cell, plan distance to the centreline, m (Infinity: none)
};

// R8, R10, R18 — press the finished line into the ground. Writes `ground` in place.
Corridor stamp_corridor(const Piste& piste, ge::core::Heightfield& ground);

}  // namespace fall_line::mapgen
