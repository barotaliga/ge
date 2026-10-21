// THE DISCIPLINES' RULE BOOK (R31 onward): the numbers a RACE COURSE of a discipline is set to on
// a map that is already built (R1–R30). The map is the mountain; a discipline is how a race uses
// it: where on the piste its course starts and finishes, which gates stand down it, what the
// organisers do to the hill first (`CoursePrep`, course_prep.hpp). The slalom (R31) is set today;
// the speed events, speed skiing and the ski cross follow (docs/goals.md M4b4).
//
// THE RESEARCH BEHIND THE SLALOM'S NUMBERS, the international alpine competition rules by
// article: a top-level slalom hill drops 140–220 m (lower races 120–200, children 100–160, an
// entry league 80–120) on a 33–45 % gradient; a gate is two poles 4–6 m apart, the TURNING pole
// a flex pole the racer turns round, and flies no panel; turning pole to turning pole of two
// gates is 6–13 m; the direction changes are 30–35 % of the vertical in metres, ±3; a course
// carries at least three HAIRPINS (two closed gates) and one to three VERTICALS (three or four),
// 0.75–1 m between the gates of one, and one to three DELAYED gates 12–18 m on; no hard gate at
// the start or the finish, an easier gate before a combination, the last gate fast and aimed at
// the finish's middle, the finish at least 10 m wide; two runs on the same slope.
//
// R31 THE SLALOM COURSE. A slalom is set on a built map's piste, on the stretch of it that drops
//     one of `slalom::drops` metres, the steepest to the top level's gradient and then the
//     longest drop (`slalom::pick`; a drop under `slalom::pick::least` only where none over it
//     fits), between a START and a FINISH, no longer than `slalom::max_length`, nowhere narrower
//     than `slalom::min_width`, crossing no drop on the piste, its finish on a gentler run-out (no
//     steeper than `slalom::outrun` over the `slalom::outrun_length` past the line): the start a
//     hut over the piste, the snow out of its door cut into a START DROP as steep as
//     `slalom::drop.grade` over its first `slalom::drop.length` metres, the line
//     `slalom::finish_width` metres wide at the least, every kicker on the piste within
//     `slalom::clearance` of the stretch levelled, the piste groomed hard from the hut to the end
//     of the run-out whatever drift lay across it and its relief combed smooth over
//     `slalom::comb`, and every tree within `slalom::clear` metres of the course and in the finish
//     arena cleared. Its gates are POLE GATES, red and blue alternately, as many direction
//     changes as `slalom::changes` of the vertical (spaced `slalom::spacing` metres apart down the
//     piste, which wins where the two disagree): OPEN gates across the hill, the turning pole set
//     either side of the course's line in turn, no further across than leaves the line round the
//     poles bending no tighter than `slalom::bend`, the outside pole `slalom::width` metres
//     further out, `slalom::turn` metres turning pole to turning pole; `slalom::hairpins` HAIRPINS
//     and `slalom::verticals` VERTICALS of CLOSED gates, poles one above the other,
//     `slalom::closed` metres apart, `slalom::gap` between the gates of one, in a line down the
//     hill; and `slalom::delays` DELAYED gates `slalom::delayed` metres on. No combination stands
//     within `slalom::clean` gates of the start or the finish, an open gate comes before every
//     one, every pole stands `slalom::inside` metres inside the piste, and the last gate is
//     `slalom::last` metres above the line and aims at its middle. A slalom is two RUNS on the
//     same stretch, the second set afresh.
//
// Every value is a decimal literal, so the doubles are fixed bits. Held by
// tests/mountain_slalom_test.cpp and the analysis (analysis/race.hpp).
//
// ge:summary The disciplines' rule book (R31-): the slalom's numbers and what a course prepares
// ge:keywords discipline-rules rule-book slalom course-prep start-drop comb arena hairpin
// ge:keywords vertical-combination delayed-gate pole-gate r31
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#pragma once

#include <array>

#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

// WHAT A DISCIPLINE ASKS OF THE HILL before its course is set (course_prep.hpp), every length in
// metres.
struct CoursePrep {
  double clearance;      // how far either side of the stretch a kicker on the piste is levelled
  double outrun_length;  // the finish's run-out past the line, groomed with the course
  double stand;          // how far above the wand's line the racer stands in the house
  // The combing: over how far either side of each point the ground is taken to its mean, and how
  // far past the piste's edge it eases out.
  struct Comb {
    double reach;
    double ease;
  } comb;
  // The start drop: level for `lip`, steepened to `grade` at its steepest over the next `length`
  // (never more than `most` deep), eased back over `ease`, its banks `shoulder` wide either side
  // of the piste.
  struct Drop {
    double lip;
    double length;
    double grade;
    double most;
    double ease;
    double shoulder;
  } drop;
  double clear;  // trees cleared within this of the piste's edge along the course
  // The finish arena's box past the line: along, back up, across (half).
  struct Arena {
    double past;
    double before;
    double half;
  } arena;
};

namespace discipline_rules::slalom {

// The vertical drops a slalom is set to, m, tried in order: the top level's, then a lower race's,
// then an entry league's.
inline constexpr std::array<double, 5> drops{190, 160, 140, 110, 80};
// Which stretch wins: its gradient counted to the top level's steepest (`steep`), then
// `vertical` a metre of drop and `low` for the foot of the piste, so a steep 140 m beats a gentle
// 190 m. The drops from `least` up compete so; a shorter one is tried only where none fits.
namespace pick {
inline constexpr double steep = 0.45;
inline constexpr double vertical = 0.0004;
inline constexpr double low = 0.03;
inline constexpr double least = 140;
}  // namespace pick
inline constexpr Band vertical{140, 220};
// The longest a slalom runs down the piste, m: a top-level slalom hill is about 590 m long.
inline constexpr double max_length = 720;
// The least piste width along it, m.
inline constexpr double min_width = 15;
// The finish's run-out: the steepest mean gradient past the line, and over how far, m.
inline constexpr double outrun = 0.22;
inline constexpr double outrun_length = 40;
// The line's least width, m.
inline constexpr double finish_width = 27;
// Trees cleared within this of the piste's edge along the course, m.
inline constexpr double clear = 14;
// How far the stretch keeps from a drop on the piste, m, and how far either side of it a kicker
// on the piste is levelled away.
inline constexpr double clearance = 30;
// The direction changes as a share of the vertical, and the ± on it.
namespace changes {
inline constexpr double min = 0.3;
inline constexpr double max = 0.35;
inline constexpr int slack = 3;
}  // namespace changes
// The spacing of the gates down the piste, m (plan).
inline constexpr Band spacing{10.5, 12.5};
// An open gate: the turning pole either side of the course's line, m (a slalom ski's carve at
// race pace, a section tight and a section open), and the gate's width to its outside pole, m.
inline constexpr Band across{0.5, 1.0};
inline constexpr Band width{4, 6};
// Turning pole to turning pole, m.
inline constexpr Band turn{6, 13};
// A closed gate's poles apart down the hill, m, and the gap between the gates of one
// combination, m.
inline constexpr Band closed{4.8, 5.6};
inline constexpr Band gap{0.75, 1};
// THE TIGHTEST THE LINE ROUND THE POLES MAY BEND, m: a slalom ski's carve at race pace with its
// edge change between two turns, so an open gate's turning pole stands no further across than
// the gates either side of it leave room to swing to.
inline constexpr double bend = 8;
// How many of each combination a course carries.
inline constexpr std::array<int, 2> hairpins{3, 4};
inline constexpr std::array<int, 2> verticals{1, 2};
inline constexpr std::array<int, 2> delays{1, 2};
// A delayed gate's turning pole on from the last, m, and how far across, m.
inline constexpr Band delayed{12.5, 16};
inline constexpr Band delay_across{1.8, 2.4};
// No combination within this many gates of the start or the finish.
inline constexpr int clean = 3;
// Every pole this far inside the piste's edge, m.
inline constexpr double inside = 1;
// The last gate above the finish line, m.
inline constexpr double last = 16;

// What a slalom asks of the hill: the racer stood 1.1 m above the wand; the start drop level for
// 0.6 m, then as steep as 0.75 over 8 m (at most 3.5 m deep), eased back over 40 m with 10 m
// banks; the hill combed of the lips a black's face carries (one every 5–7 m) over 6 m either
// side; the finish arena 70 m past the line, 25 m back up it and 40 m either side.
inline constexpr CoursePrep prep{
    clearance, outrun_length, 1.1, {6, 6}, {0.6, 8, 0.75, 3.5, 40, 10}, clear, {70, 25, 40},
};

}  // namespace discipline_rules::slalom

}  // namespace ge::mountain::mapgen
