// R28 — THE COURSES: the line a skier follows from a run's top station down the network to the
// village, which is what a map of a ski area is raced on.
//
// A course is the run it starts on, then the run that one merges into from the arc it joins at,
// and so on until a run reaches the floor. The runs were walked onto each other's lines, so where
// one joins the next they already run together a stride apart; the course eases across that last
// stride over `course.merge` m (a lane change down a groomed surface both runs share) and is then
// laid again every `track.step` m from the top, like any piste, since the piste queries count on
// even stations. Each piece remembers where on the course it lies, so a kicker, a drop or a drift
// a run carries can be named by its arc down the course (`course_arc`).
//
// ge:summary R28 a ski area's courses: runs chained down the network to the village, resampled
// ge:keywords courses course compose-course course-arc piece merge junction village
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <span>
#include <string>
#include <variant>
#include <vector>

#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// One run's stretch of a course: the run (its place in the runs handed in), the arcs on it the
// course runs between, and the course's own arcs there, m.
struct CoursePiece {
  int run;
  double from;
  double to;
  double at;
  double until;
};

// A course as it was composed: the run it starts on, its pieces in the order skied, its points
// every `track.step` m (heights left at nought for the caller to sample) and its length, m.
struct CoursePlan {
  int start = 0;
  std::vector<CoursePiece> pieces;
  std::vector<TrackPoint> points;
  double length = 0;
};

// R28 — the course down the network from run `start`'s top: a plan, or why there is none (a run
// joined that was never laid, or a chain that loops back onto a run it skied).
std::variant<CoursePlan, std::string> compose_course(std::span<const WalkedRun* const> runs,
                                                     int start);

// The course's arc at run `run`'s arc `s`, or a negative number where the course does not ski
// that stretch of that run.
double course_arc(const CoursePlan& course, int run, double s);

}  // namespace ge::mountain::mapgen
