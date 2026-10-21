// R27 — THE RUNS, BUILT: each walked run graded into the mountain, in the order the runs were
// laid, so a run that merges into another is graded onto the other's finished surface.
//
// THE GRADING is R8's (track.hpp's `grade_piste`): the line made monotone by the cut from above
// and the fill from below and their mean, then held under the run's ceiling and its colour's
// (R23) the same way, blurred round the joins, the finish straight laid flat on a run that
// reaches the floor. What a resort adds is the JUNCTION: a run that merges into another is
// graded onto the other's surface from where its corridor touches the other's, eased in over
// the stretch before, keeping the cross-fall the other's surface already has there.
//
// Held by tests/mountain_resort_build_test.cpp (each walked run's heights and cross-fall).
//
// ge:summary R8 R27 a walked run graded onto the ground and onto the run it merges into
// ge:keywords network-build grade-run funnel-into junction plane pin camber cross-fall
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <string>

#include "core/heightfield.hpp"
#include "mountain/mapgen/network.hpp"

namespace ge::mountain::mapgen {

// How a run is graded where the rule book's own numbers do not hold (a real face, R25): the
// least it falls, m per m, the deepest it is cut or filled, m, and whether its start is cut
// through the ground rather than filled over it.
struct GradeFace {
  double fall = rules::track::min_grade;
  double cut = rules::track::max_cut;
  bool head = false;
};

// Whether a plan point stands on the core of a run already pressed.
using PinnedAt = std::function<bool(double x, double z)>;

// R8, R27 — grade a run's profile against `ground`: no window steeper than `max_grade`, no
// colour window steeper than `colour` (none on a black or a lane), the finish flat on a run
// that reaches the floor, and on one that merges the line drawn onto `onto`'s surface. Writes
// each station's y, `raw` and `cross`; returns why a line cannot be graded. `stepped` grades a
// junction as the earliest generator did.
std::optional<std::string> grade_run(WalkedRun& run, const ge::core::Heightfield& ground,
                                     double max_grade, std::optional<double> colour,
                                     const WalkedRun* onto, const PinnedAt* pinned = nullptr,
                                     bool stepped = false, const GradeFace& face = {});

// THE FUNNEL (R27): a merging run wider than the run it merges into narrows to that one's width
// as it closes on it. Narrows the widths in place.
void funnel_into(WalkedRun& run, const WalkedRun& onto);

}  // namespace ge::mountain::mapgen
