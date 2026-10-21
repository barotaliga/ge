// THE DISCIPLINES' RULE BOOK (R31 onward): the numbers a RACE COURSE of a discipline is set to on
// a map that is already built (R1–R30). The map is the mountain; a discipline is how a race uses
// it: where on the piste its course starts and finishes, which gates stand down it, what the
// organisers do to the hill first (`CoursePrep`, course_prep.hpp). The slalom (R31), the downhill
// (R32), the super-G (R33), the giant slalom (R36) and the speed-skiing track (R34) are set
// today; the ski cross follows (docs/goals.md M4b4).
//
// The research behind each discipline's numbers and its rule, by number, stand below the includes.
//
// ge:summary The disciplines' rule book (R31-R34, R36): each course's numbers, what it prepares
// ge:keywords discipline-rules rule-book slalom course-prep start-drop comb arena hairpin
// ge:keywords vertical-combination delayed-gate pole-gate r31 downhill super-g giant-slalom
// ge:keywords turn-rule line-rule trap-rule speed-trap a-nets r32 r33 r36
// ge:keywords speed-skiing speed-ski timing-zone run-out launch r34
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#pragma once

#include <array>

#include "mountain/mapgen/rules.hpp"

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
// THE RESEARCH BEHIND THE SPEED EVENTS' NUMBERS, the same rules: a downhill drops 450–1100 m
// (the men's top level 800–1100, the women's 450–800) and sets no length (measured courses run
// about 3.5 km over 860 m with some 40 gates); its gates are four poles and two red panels at
// least 8 m between the inner poles, marking the racing line every 80 m or so (measured median)
// rather than turning the racer; the course is about 30 m wide, its obstacles behind 4–6 m safety
// nets, its finish at least 15 m wide; one run after a training run. A super-G drops 350–650 m
// (measured top-level courses 598 ± 38 m over about 2.3 km); its direction changes are at least
// 7 % of the vertical, turning pole to turning pole at least 25 m (measured 49.5 ± 5.7 m along
// the course, 12–13 m across); a gate is four flex poles and two panels 6–8 m apart, red and
// blue in turn; one run, no training. A giant slalom drops 250–450 m; its direction changes are
// 11–15 % of the vertical; turning pole to turning pole at least 10 m (measured 26–27 m down the
// hill, 6.8 ± 2.4 m across); a gate is four poles and two panels 4–8 m apart; two runs.
//
// R32 THE DOWNHILL COURSE. A downhill is set on a built map's WHOLE piste, from its start gate
//     to its finish line, on the course of the ski area with the most vertical
//     (`downhill::vertical` the band it is held to where the area has one): the start a house
//     over the piste, the snow out of its door cut into a START DROP as steep as
//     `downhill::prep.drop.grade` over its first `downhill::prep.drop.length` metres, every
//     kicker on the piste levelled, the piste groomed hard from the house to the end of the
//     run-out and its relief combed smooth, its rollers and its drops kept as the course's JUMPS,
//     every crest sharper than `downhill::prep.crest` metres of radius shaved round so a racer at
//     speed comes down on the slope below it, every tree within `downhill::prep.clear` metres of
//     the piste and in the finish arena cleared, and A-NETS along both edges `downhill::nets.gap`
//     metres outside the piste from the house to the finish arena. Its RACING LINE is the line
//     down the piste that bends the least, kept `downhill::line.margin` metres inside its edges,
//     and its gates MARK it: SPEED GATES, four poles and two red panels, `downhill::width` metres
//     between the inner poles, centred on the line, every `downhill::spacing` metres down the
//     piste, none within `downhill::jump` metres of a jump. The finish line is
//     `downhill::finish_width` metres wide at the least. The SPEED TRAP stands where the piste
//     has fallen the most over the `downhill::trap.run` metres before it, on a straight, in the
//     course's last `downhill::trap.late` share. A downhill is one RUN.
//
// R33 THE SUPER-G COURSE. A super-G is set on the piste of the ski area's course with the most
//     vertical, from a START LOWERED down it until the drop to the finish line is
//     `super_g::rule.target` metres at the most, prepared as a downhill's is (its own start drop,
//     combing and crests, `super_g::rule.prep`), with A-NETS along both edges. Its JUMPS are its
//     drops and the crests a racer leaves the snow on (the profile still bending over tighter
//     than `takeoff` metres). Its gates TURN the racer: as many as `changes` of the vertical in
//     metres and `spacing` metres apart down the piste give, whichever is more, each strayed off
//     the even spacing by `rhythm`, none within `jump` metres before a jump nor `landing` after
//     it. Its RACING LINE is the line that bends the least kept `line.margin` metres inside the
//     piste and `line.most` off its middle, SWUNG to each gate's side in turn by `swing` metres,
//     no further than bends it tighter than `bend` metres nor leaves the gate off the snow;
//     `opening` of it at the first gates, the `straight` share before a jump, and less on a
//     gentle stretch (`flat`), eased from one gate's apex to the next. Each gate is FOUR poles and
//     TWO panels, red and blue in turn, `width` metres between the inner poles, its turning pole
//     `pass` metres inside the line's apex and every pole `inside` metres inside the piste. The
//     finish line is `finish_width` metres wide at the least; the SPEED TRAP stands as a
//     downhill's does (`super_g::trap`). A super-G is one RUN.
//
// R36 THE GIANT-SLALOM COURSE. A giant slalom is set as a super-G is (R33) with a technical
//     race's numbers (`giant_slalom::rule`): a start lowered to its own target, closer gates, a
//     tighter swing, a shorter start drop, nets along both edges and no speed trap. It is two
//     RUNS on the same stretch, the second's gates and swing dealt afresh. (Its line held off a
//     lift tower standing on the piste waits for the towers, which the map does not publish yet.)
//
// THE RESEARCH BEHIND SPEED SKIING'S NUMBERS, speed skiing's own rules: a track is cut straight
// down a face, at least 30 m wide from above the timing zone to the run-out's end, with a 25 m
// safety margin cleared of every obstacle either side of it past 180 km/h; the racer launches
// from a start point near the top (a typical track's launch 300–400 m, the fastest tracks'
// 800–900), is timed over a 100 m zone, and is stopped on a run-out (real ones 325–840 m) whose
// braking line no racer may skid or turn before; the top class's tour winners go 180–230 km/h;
// neighbouring start points may give no more than 15 km/h apart; a race is a qualification from
// a lower start, then a final from the top.
//
// R34 THE SPEED-SKIING TRACK. A speed track is cut STRAIGHT down a built map's face, its own line
//     and not the piste: a straight line from a column of the face (`speed_ski::search`), its
//     bearing within a few degrees of the fall line, whose GRADED PROFILE (the ground across the
//     track's width taken to its mean along it over `speed_ski::smooth` metres, never rising,
//     every crest rounded to no tighter than `speed_ski::crest` metres of radius and every knee
//     to no tighter than `speed_ski::knee`) lies within `speed_ski::cut` metres of the ground
//     under its middle. Down it, from `speed_ski::platform` metres above the final's start house:
//     a LAUNCH of one of `speed_ski::launch` metres along the snow, the TIMING ZONE of
//     `speed_ski::trap` metres along the snow, and a RUN-OUT that stops a racer stood up into the
//     wind and skidding past its braking line (`speed_ski::stop`, `speed_ski::run_out`) before
//     the line leaves the map, the track running `speed_ski::past` metres beyond to the finish
//     enclosure. Of the lines and launches that fit, the one whose final the top class skis
//     nearest the speed the map's track is built for (dealt off its seed in
//     `speed_ski::speed::aim`, inside `speed_ski::speed` where one is) wins, its trap on a gentle
//     stretch, its launch near a typical track's and its run-out no longer than a real one's
//     (`speed_ski::fit`), then the least graded. The track is `speed_ski::width` metres wide,
//     graded to its profile across that width and eased out over `speed_ski::ease`, groomed hard,
//     every tree within `speed_ski::margin` metres of its edges (the safety margin) and in the
//     finish enclosure (`speed_ski::arena`) cleared, and the piste's kickers and drops in the cut
//     taken out; no lift station stands within `speed_ski::stations` metres of it. Its gates are
//     the START GATE under the house and the timing zone's two LINES across the track and its
//     margins. A race is two RUNS: the QUALIFICATION from a start lowered down the track until it
//     gives `speed_ski::qualify` km/h less, and the FINAL from the top.
//
// Every value is a decimal literal, so the doubles are fixed bits. Held by
// tests/mountain_slalom_test.cpp and the analysis (analysis/race.hpp).

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
  // THE CRESTS SHAVED (a speed event's): no crest down the course sharper than this radius, m, so
  // a racer flown off it at speed comes down on the slope below it. 0: no crest is cut.
  double crest = 0;
};

// HOW A RACING LINE IS LAID (speed_course.hpp): a station every `step` m, kept `margin` m inside
// the piste's edges and, where `most` is over 0, never more than `most` m off its centreline,
// the room it is held in opening and closing no faster than `ease` m a metre, relaxed over
// `passes` at each of the `strides`, coarse to fine.
struct LineRule {
  double step;
  double margin;
  double most;
  double ease;
  int passes;
  std::array<int, 7> strides;
};

// WHERE A SPEED TRAP STANDS: where the piste has fallen the most over the `run` m before it, on a
// straight (no bend tighter than `straight` m of radius within `span` m either side), in the
// course's last `late` share and `end` m short of the finish at the least.
struct TrapRule {
  double run;
  double straight;
  double span;
  double late;
  double end;
};

// How a speed course's gates are spaced down the piste, m: the least, the most, the aim.
struct SpacingRule {
  double min;
  double max;
  double target;
};

// Safety nets along both edges: their line outside the piste's edge, m, and how tall, m.
struct NetRule {
  double gap;
  double height;
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
    clearance, outrun_length, 1.1, {6, 6}, {0.6, 8, 0.75, 3.5, 40, 10}, clear, {70, 25, 40}, 0,
};

}  // namespace discipline_rules::slalom

namespace discipline_rules::downhill {

// The vertical a downhill is held to, m: the women's floor to the men's top. The ski area's course
// with the most vertical is the one a downhill is set on, in the band or not.
inline constexpr Band vertical{450, 1100};
// What a downhill asks of the hill: the racer 1.1 m above the wand; a speed event's start ramp,
// gentler than a slalom's cut and longer (as steep as 0.55 over 12 m, at most 3 m deep, eased
// over 50 m); every kicker within 30 m levelled; 60 m of run-out groomed; combed of its short
// lips over 6 m; no crest sharper than 80 m of radius (a racer off a knoll at 100+ km/h comes down
// on its downslope); trees cut 12 m past the edge (past the nets) and a finish arena 90 m past the
// line, 30 m back up it and 45 m either side.
inline constexpr CoursePrep prep{
    30, 60, 1.1, {6, 6}, {0.6, 12, 0.55, 3, 50, 10}, 12, {90, 30, 45}, 80,
};
// THE A-NETS: 3 m outside the piste's edge, 5 m tall.
inline constexpr NetRule nets{3, 5};
// The gates down the piste, m (the measured median's 79 m the aim).
inline constexpr SpacingRule spacing{55, 110, 80};
// Between the inner poles, m (the rule's 8 at the least), and every pole this far inside the
// piste's edge, m.
inline constexpr double width = 10;
inline constexpr double inside = 3;
// THE RACING LINE: a station every 4 m, 8 m inside the piste's edges (a gate's half-width and its
// poles' own room), the room easing at 0.1 m a metre, relaxed 60 passes a stride.
inline constexpr LineRule line{4, 8, 0, 0.1, 60, {64, 32, 16, 8, 4, 2, 1}};
// No gate within this of a jump's lip or a drop's edge, m, before or after.
inline constexpr double jump = 25;
// The finish line's least width, m.
inline constexpr double finish_width = 20;
// THE SPEED TRAP.
inline constexpr TrapRule trap{250, 400, 30, 0.6, 60};

}  // namespace discipline_rules::downhill

// WHAT A TURNING COURSE IS SET BY (turn_course.hpp): a super-G's numbers or a giant slalom's.
struct TurnRule {
  CoursePrep prep;
  Band vertical;  // the band the drop is held to where the piste has one, m
  double target;  // the start lowered until the drop to the finish is this at the most, m
  NetRule nets;
  double changes;  // the direction changes, a share of the vertical in metres
  SpacingRule spacing;
  double rhythm;                  // how far a gate's spacing strays from the even one, a share
  double width;                   // between the inner poles, m
  double inside;                  // every pole this far inside the piste's edge, m
  double pass;                    // the line this far outside the turning pole, m
  std::array<double, 2> opening;  // the first gates' share of their own swing
  // A gate on a GENTLE stretch (the piste falling less than `to` over the `over` m above it)
  // swings less, down to `least` of its own at `from`.
  struct Flat {
    double over;
    double from;
    double to;
    double least;
  } flat;
  Band swing;       // how far the line swings to a gate's side, m
  double bend;      // the tightest the swing may bend the line, m of radius
  double approach;  // a gate within this of a jump's lip, m...
  double straight;  // ...swings this share of its own
  LineRule line;
  double takeoff;     // a crest a racer flies off bends tighter than this, m of radius...
  double crest_span;  // ...read over this either side, m
  double jump;        // no gate within this before a jump, m...
  double landing;     // ...nor this after it, m
  double finish_width;
};

namespace discipline_rules::super_g {

// The super-G: a speed ramp a little shorter (it turns from its first gates), crests shaved to
// 65 m of radius (flown at 25–28 m/s, where a racer leaves the snow over a crest tighter than
// v²/g), held to 350–650 m and lowered to 600 m; the A-nets; at least 7 % direction changes;
// gates 32–75 m apart aiming at 49 m, straying 18 %; 8 m between the inner poles, 2 m inside the
// piste, the line 3.5 m outside the turning pole; the first two gates swinging 35 % and 70 % of
// their own; a gentle stretch (under 8–20 % over 80 m) swinging down to 35 %; a swing of
// 2.5–5.5 m bending no tighter than 50 m; a gate within 70 m of a jump swinging 30 %; the line
// 9 m inside the piste and at most 5 m off its middle; a crest flown off where the profile bends
// tighter than 110 m over 10 m, no gate 25 m before it nor 45 m after; the finish 20 m wide.
inline constexpr TurnRule rule{
    {30, 60, 1.1, {6, 6}, {0.6, 10, 0.55, 3, 45, 10}, 12, {90, 30, 45}, 65},
    {350, 650},
    600,
    {3, 5},
    0.07,
    {32, 75, 49},
    0.18,
    8,
    2,
    3.5,
    {0.35, 0.7},
    {80, 0.08, 0.2, 0.35},
    {2.5, 5.5},
    50,
    70,
    0.3,
    {4, 9, 5, 0.1, 60, {64, 32, 16, 8, 4, 2, 1}},
    110,
    10,
    25,
    45,
    20,
};
// THE SPEED TRAP.
inline constexpr TrapRule trap{200, 300, 25, 0.5, 60};
// The least between two gates' turning poles, m (the rule's).
inline constexpr double apart = 25;

}  // namespace discipline_rules::super_g

namespace discipline_rules::giant_slalom {

// The giant slalom: a shorter, gentler start drop (as steep as 0.5 over 8 m, at most 2.5 m deep,
// eased over 35 m); crests shaved to 45 m (skied at 18–22 m/s, on the snow); held to 250–450 m and
// lowered to 400 m; nets 3 m out and 4 m tall; 13 % direction changes (the rule's 11–15 % at its
// middle); gates 18–42 m apart aiming at 27 m, straying 20 %; 6 m between the inner poles, 2 m
// inside the piste, the line 3 m outside the turning pole; the first two gates swinging 50 % and
// 80 %; a gentle stretch (under 8–20 % over 60 m) swinging down to 45 %; a swing of 2.5–4.5 m
// bending no tighter than 22 m (a giant slalom's ~20 m turn); a gate within 40 m of a jump
// swinging 40 %; the super-G's line; a crest flown off where the profile bends tighter than 60 m
// over 8 m, no gate 20 m before it nor 35 m after; the finish 20 m wide.
inline constexpr TurnRule rule{
    {30, 60, 1.1, {6, 6}, {0.6, 8, 0.5, 2.5, 35, 10}, 12, {80, 30, 45}, 45},
    {250, 450},
    400,
    {3, 4},
    0.13,
    {18, 42, 27},
    0.2,
    6,
    2,
    3,
    {0.5, 0.8},
    {60, 0.08, 0.2, 0.45},
    {2.5, 4.5},
    22,
    40,
    0.4,
    {4, 9, 5, 0.1, 60, {64, 32, 16, 8, 4, 2, 1}},
    60,
    8,
    20,
    35,
    20,
};
// The least between two gates' turning poles, m (the rule's).
inline constexpr double apart = 10;

}  // namespace discipline_rules::giant_slalom

namespace discipline_rules::speed_ski {

// The track's width, m (the rule's least from above the trap to the run-out's end), and the
// SAFETY MARGIN cleared of every tree either side of it, m.
inline constexpr double width = 30;
inline constexpr double margin = 25;
// How far past the track's edge its grading eases out, m.
inline constexpr double ease = 14;
// THE LAUNCHES tried from the final's start to the timing zone's top line, m along the snow.
inline constexpr std::array<double, 7> launch{300, 400, 500, 600, 700, 800, 900};
// THE TIMING ZONE, m along the snow.
inline constexpr double trap = 100;
// The final's speed through the trap, km/h: the band a track is held in, and the band the speed
// a track is BUILT FOR is dealt in off its map's seed (one track slow, another fast).
namespace speed {
inline constexpr double min = 180;
inline constexpr double max = 240;
inline constexpr Band aim{185, 235};
}  // namespace speed
// THE QUALIFICATION'S START, lowered down the track until it gives this much less, km/h.
inline constexpr double qualify = 12;
// THE PROFILE: the mean across the track taken along it over this many metres either side;
// every crest no tighter than `crest` m of radius (a racer at 65 m/s leaves the snow over a crest
// tighter than v²/(g cos θ), some 480 m on the steep); every KNEE no tighter than `knee` m (at
// 55 m/s some 0.9 g on the legs, a compression a racer holds his tuck through); and no point of
// the profile more than `cut` m from the ground under the track's middle.
inline constexpr double smooth = 30;
inline constexpr double crest = 600;
inline constexpr double knee = 350;
inline constexpr double cut = 9;
// How far the track runs on up the slope above the final's start, m (the house stands on it),
// and how far above the wand the racer stands, m.
inline constexpr double platform = 10;
inline constexpr double stand = 1.1;
// THE STOP the run-out is sized for: a racer of the top class (107 kg, the drag areas of a tuck
// and of a body stood up into the wind, m²) stood up past the trap, skidding at `skid` g once
// under `below` m/s past the braking line, stopped at `rest` m/s; the base's rolling friction
// and the air's density, kg/m³.
struct Stop {
  double mass, tuck, stood, friction, air, skid, below, rest;
};
inline constexpr Stop stop{107, 0.08, 0.6, 0.035, 1.05, 0.15, 40, 3};
// WHICH FIT WINS beside the speed (each a km/h's worth of miss): the trap on a gentle stretch
// (`trap` a unit of grade past `trap_grade`), a launch near `launch_aim` (`launch` a metre off
// it), a run-out no longer than `outrun_most` (`outrun` a metre past it), and the least grading
// (`cut` a metre).
struct Fit {
  double trap, trap_grade, launch, launch_aim, outrun, outrun_most, cut;
};
inline constexpr Fit fit{100, 0.3, 0.01, 400, 0.02, 800, 0.5};
// THE RUN-OUT as a racer home rides it: untucking over the first `untuck` m past the zone, the
// BRAKING LINE `brake` m past it, skidding hard only under `below` m/s.
namespace run_out {
inline constexpr double untuck = 120;
inline constexpr double brake = 150;
inline constexpr double below = 40;
}  // namespace run_out
// How far the track runs past the stop, m: the finish enclosure.
inline constexpr double past = 120;
// THE SEARCH: the columns tried across the face, `stride` m apart and `edge` m in from the map's
// sides; the bearings tried either side of the fall line, degrees; where on the face a line is
// read from, m down from its top edge; the step it is read in, m; every how many steps a start
// is tried.
namespace search {
inline constexpr double stride = 50;
inline constexpr double edge = 150;
inline constexpr std::array<double, 9> bearings{-20, -15, -10, -5, 0, 5, 10, 15, 20};
inline constexpr double top = 250;
inline constexpr double step = 4;
inline constexpr int starts = 6;
}  // namespace search
// No lift station within this of the track's edge, m.
inline constexpr double stations = 40;
// The finish enclosure's box past the stop, m: along, back up, across (half).
struct Arena {
  double past, before, half;
};
inline constexpr Arena arena{90, 40, 45};

}  // namespace discipline_rules::speed_ski

}  // namespace ge::mountain::mapgen
