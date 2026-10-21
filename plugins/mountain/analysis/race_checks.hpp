// THE CHECKS EVERY RACE COURSE SHARES, and each discipline's own beside them, behind
// `analyze_race` (race.hpp): a gate's poles, the hill prepared for the course (no kicker left on
// it, groomed hard from the house to the run-out, the woods cut), and the speed events' checks
// (speed_race.cpp). Each files its findings through `add` in a fixed order, with fixed text.
//
// ge:summary The checks every race course shares, and the speed events' (R32, R33, R36)
// ge:keywords analysis race-course course-prep gate-poles groomed cleared-trees speed-event
// ge:skill mountain
// ge:test tests/mountain_speed_events_test.cpp
#pragma once

#include <string>

#include "mountain/analysis/index.hpp"
#include "mountain/mapgen/discipline_rules.hpp"

namespace ge::mountain::analysis {

// A gate's two poles, in plan: the first its turning pole on a gate that turns the racer.
struct GatePoles {
  mapgen::PlanPoint a;
  mapgen::PlanPoint b;
};
GatePoles poles_of(const mapgen::Checkpoint& g);

inline std::string gate_name(size_t i) { return "gate " + std::to_string(i); }

// THE HILL PREPARED for `race` by `P`, filed under `rule`: no kicker on the piste within
// `P.clearance` of the course, the snow groomed hard from the wand to the end of the run-out, and
// no tree within `P.clear` of the piste's edge along the course.
void check_prepared(const mapgen::Level& level, const mapgen::RaceCourse& race,
                    const mapgen::CoursePrep& P, const char* rule, const Add& add);

// R32 — the downhill: its stretch, its speed gates on the line, their spacing, the line, the
// crests shaved, the nets and the speed trap, the hill prepared.
void check_downhill(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add);

// R33, R36 — a super-G or a giant slalom: the start lowered into its band, the gates (as many as
// the direction changes ask, spaced, side to side in turn, off the jumps), each turning pole
// inside the line's apex, the crests shaved, the nets (and a super-G's trap), the hill prepared.
void check_turns(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add);

}  // namespace ge::mountain::analysis
