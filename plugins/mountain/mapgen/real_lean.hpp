// R25 — LEANING ON A REAL FACE'S HINTS: the generator's stations, lifts and runs moved toward the
// real ski area's (real_hints.hpp) where its own rules allow, by the reaches in
// `resort_rules::massif::real`. Every function here is a pure function of the hints and what it
// is handed; none draws on a stream, so leaning moves nothing a dealt massif is dealt.
//
//   * A STATION (a lift's top or bottom) moves onto the nearest real lift's end of the same kind
//     of end that no other station took (`lean_station`).
//   * A LIFT moves onto the real lift of its kinds whose line passes nearest both its ends: its
//     top onto the real top, its bottom onto the real bottom, or, where the bottom is held on the
//     valley floor (below where most real lifts start), onto the real line carried down to it
//     (`lean_lift`).
//   * A PISTE SLOT is laid ON the real piste leaving nearest its start that falls most of the way
//     to its target (`piste_for`), taking its colour, its line (`along_piste`) and its width
//     (`piste_width`); or, where none is near, steered through the bends of a real piste of its
//     own colour (`piste_via`) and, between them, toward the nearest real piste ahead
//     (`piste_ahead`).
//   * A RUN laid on a real piste is BILLED the colour the real piste is signed where it measures
//     within `least::billed` colours of it (`billed_colour`).
//
// ge:summary R25 the generator leaning its stations, lifts and runs onto a real face's hints
// ge:keywords real-hints lean-station lean-lift piste-for piste-via piste-ahead along-piste
// ge:keywords piste-width billed-colour r25
// ge:skill mountain
// ge:test tests/mountain_real_hints_test.cpp
#pragma once

#include <functional>
#include <initializer_list>
#include <optional>
#include <vector>

#include "mountain/mapgen/real_hints.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// Which of the hints' lifts and pistes a station or a run has already taken.
struct HintsUsed {
  std::vector<bool> lifts;
  std::vector<bool> pistes;
  explicit HintsUsed(const RealHints& h) : lifts(h.lifts.size()), pistes(h.pistes.size()) {}
};

// Where a run steers at (x, z) to follow a real piste, or nothing where none is near.
using HintSteer = std::function<std::optional<PlanPoint>(double, double)>;
// The real width of the pistes at (x, z), m; 0 where the map gives none near.
using HintWidth = std::function<double(double, double)>;

// A station at `at` moved onto the nearest untaken real lift's `top` (or bottom) end within
// `massif::real::station::reach`, its row moved at most `station::rise` (left as it is with
// `keep_z`: a top on the summit ridge). Unchanged where no real end is near.
PlanPoint lean_station(const RealHints& hints, HintsUsed& used, PlanPoint at, bool top,
                       bool keep_z = false);

// A lift from `bottom` to `top`, one of `kinds`, moved onto the untaken real lift whose line
// passes nearest its two ends, each within `massif::real::lift::reach`: its top onto the real
// one's (its row moved at most `lift::rise`, or held with `keep_top`), its bottom onto the real
// one's, or, with `keep_bottom`, onto the real line carried down to its row, no further than
// `lift::carry`. Nothing where none is near.
struct LeantLift {
  PlanPoint bottom;
  PlanPoint top;
};
std::optional<LeantLift> lean_lift(const RealHints& hints, HintsUsed& used, PlanPoint bottom,
                                   PlanPoint top, std::initializer_list<HintLiftKind> kinds,
                                   bool keep_bottom, bool keep_top);

// The untaken real piste a run leaving `start` for `target` is laid on: the one whose top is
// nearest within `massif::real::along::reach` and that falls `along::fall` of the way to the
// target's row. A ski route is never one. Null where none is near.
const HintPiste* piste_for(const RealHints& hints, HintsUsed& used, PlanPoint start,
                           PlanPoint target);

// The bends a run leaving `start` for `target` built to `grade` steers through: those of the
// untaken real piste whose top is nearest within `via::reach` (one of `grade` `via::same` m
// nearer) and that falls `via::fall` of the way to the target's row, kept `via::margin` clear of
// both rows. Empty where none is near.
std::vector<PlanPoint> piste_via(const RealHints& hints, HintsUsed& used, PlanPoint start,
                                 PlanPoint target, PisteGrade grade);

// Where a run at (x, z) steers to follow the real pistes: the nearest point on any of them
// `follow::ahead` m further down the face and no more than `follow::aside` m across (one of
// `grade` `follow::same` m nearer).
HintSteer piste_ahead(const RealHints& hints, std::optional<PisteGrade> grade);
// …and to stay on ONE real piste: the nearest point of its line `along::ahead` m further down,
// no more than `along::aside` m across.
HintSteer along_piste(const HintPiste& piste);

// The real width of the pistes near (x, z): the width the map gives at the nearest bend within
// `massif::real::width` m.
HintWidth piste_width(const RealHints& hints);

// The colour a run is BILLED: its real piste's signed colour where it measures within
// `least::billed` colours of it, else what it measures.
PisteGrade billed_colour(std::optional<PisteGrade> signed_as, PisteGrade measured);

// A hint's grade as a piste colour; none for a ski route.
std::optional<PisteGrade> piste_grade_of(HintGrade g);

}  // namespace ge::mountain::mapgen
