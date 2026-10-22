// THE VENUES' RULE BOOK (R37 onward): the rules a FREESTYLE VENUE is built to on a map that is
// already built (R1–R30), beside the races' (discipline_rules.hpp, R31–R36). A venue is not a
// course on the piste: it is a thing BUILT in the snow, shaped by the snowcats out of a
// mountain's side, on a STRAIGHT LINE of its own down the face (R37, straight_venue.hpp). The SKI
// JUMPING HILL (R43, ski_jump.hpp) is built today.
//
// THE RESEARCH BEHIND THE SKI JUMPING HILL: the hill construction norm states a hill element by
// element from a few free choices (K's distance, the hill size, the in-run's and the table's
// angles, the steepness h/n, the speed off the edge, the landing slope at K), every other element
// a formula over those (below, `ski_jump::norm`); certificates of large (HS 140) and normal
// (HS 106) hills of the class the top series jumps give the bands they are chosen in.
//
//   R37 THE STRAIGHT VENUE. A freestyle venue is BUILT straight down a built map's face, a line
//       of its own and not the piste, from a column of the face (its rule's `search`) and a
//       bearing within a few degrees of the fall line, where its profile lies nearest the
//       ground it is cut from (its rule's `fit`) and clear of every lift station and the
//       village. Its PROFILE is designed against the horizontal, the same on every map, and only
//       where it stands is the map's. The venue is its rule's `width` metres wide, graded to its
//       profile across that width and eased out over its `ease`, groomed hard, every tree within
//       its `margin` metres of its edges and in its finish arena cleared, and the piste's
//       kickers and drops in it taken out. Under a stretch that stands on a STRUCTURE (a raised
//       in-run) the ground is lowered to the ground the structure stands over, never raised to
//       it, the cut banked out the wider the deeper it is.
//
//   R43 THE SKI JUMPING HILL. A ski jumping hill is BUILT as R37 states (`ski_jump::search`,
//       `ski_jump::fit`, `ski_jump::width`), to one of the hills `ski_jump::hills` names (the
//       LARGE hill and the NORMAL hill), each a row of the construction norm's free choices: its
//       construction point K `w` metres down the slope from the edge, its hill size `hs`, its
//       in-run's angle `in_run`, its table's angle `table`, its steepness `steepness` (the height
//       `h` over the horizontal distance `n` from the edge to K), the speed off the edge it is
//       designed for in calm air `speed`, and the landing slope at K `slope`. Every other element
//       is the norm's formula over those (`ski_jump::norm`). Its PROFILE is built against the
//       horizontal, in order: a START PLATFORM level for `ski_jump::platform` metres; a roll of
//       `ski_jump::roll` metres of radius onto the IN-RUN's straight at its angle, its top gate
//       `ski_jump::above` metres below the roll; a TRANSITION whose curvature grows evenly from
//       nothing to the table's radius; the TABLE at its angle for its length to the EDGE; under
//       the edge the KNOLL's foot its height below it, and from there a cubic parabola leaving at
//       the foot's slope and meeting P at P's; from P through K to L an arc of the landing
//       radius; from L a TRANSITION whose radius grows evenly from the row's `r2l` to its `r2`
//       onto the level OUT-RUN at U, the out-run its length and `ski_jump::past` metres more. The
//       START GATES stand down the in-run from the top gate to the lowest, equally spaced along
//       it and never more than `ski_jump::gate_height` metres of height apart, numbered from 1 at
//       the bottom. A DISTANCE is credited along the profile from the knoll's foot, scaled so K
//       reads `w`. The hill is an ANALYTIC surface, not the map's grid, and the grid under it is
//       cut to the same surface, never above it; the in-run is a RAISED deck, `raised::half`
//       metres either side of the line, over ground laid at `raised::slope` degrees up from the
//       knoll's foot. Its gates are the START GATE at the top gate and the FINISH LINE at the
//       FALL LINE `ski_jump::fall_line` metres into the out-run; its spawn stands on the top gate.
//
// ge:summary The venues' rule book (R37, R43): the straight venue and the ski jumping hill's norm
// ge:keywords venue-rules straight-venue ski-jump jumping-hill hill-size construction-point
// ge:keywords in-run table knoll landing-radius out-run start-gate r37 r43
// ge:skill mountain
// ge:test tests/mountain_ski_jump_test.cpp
#pragma once

#include <array>

namespace ge::mountain::mapgen::venue_rules {

// Where and how a straight venue is searched for (R37): every `stride` m across the face, at each
// of `bearings` degrees off straight down it, starting every `starts` m down from `top`, read
// every `step` m of plan, `edge` m inside the map's edges.
struct Search {
  double stride;
  std::array<double, 5> bearings;
  double starts;
  double step;
  double edge;
  double top;
};
// How a line is scored and kept clear: the deepest cut or fill's weight beside the RMS, and how
// far off a lift station and the village it keeps, m.
struct Fit {
  double deepest;
  double stations;
  double village;
};
// The finish arena cleared round the finish line: m before it and past it, and its half-width.
struct Arena {
  double before;
  double past;
  double half;
};

namespace ski_jump {

// One hill's free choices (lengths m, angles degrees, speed m/s) and its scoring numbers (points
// a metre, a metre of in-run, a m/s of head and of tail wind).
struct Hill {
  double w, hs, in_run, table, steepness, speed, slope, r2l, r2;
  double metre, gate, head, tail;
};
inline constexpr Hill large{126, 140, 35, 11.2, 0.58, 26.2, 33.9, 115, 130, 1.8, 6.5, 9.9, 11.98};
inline constexpr Hill normal{95.4, 106, 35, 10.6, 0.55, 24.4, 34.0, 100, 115, 2.0, 6.6, 6.7, 8.1};

// THE NORM'S FORMULAS (lengths m, speeds m/s, angles degrees):
//   chord   a distance is 1.005 × the chord (h, n = w·sin, cos(atan h/n) / 1.005)
//   landing v_K = 0.68·v₀ + 12.44
//   radius  r_L = v_K²·w / 380, rounded to 10 m
//   p       β_P = β + 0.5·α − 2.5
//   foot    β₀ = β_P / 6
//   l       β_L = β − (1.4 / v_K)·(180/π)
//   r1      r₁ = 0.14·(v₀ + 0.95)²
//   t       t = 0.25·(v₀ + 0.95)
//   s       s = 0.025·w, at least 0.70 m
//   e1      e₁ = 92.3 − 1.517·γ + 0.426·HS
//   e2      e₂ = 67.3 − 0.944·γ + 0.331·HS
//   a       a = 18.9 + 1.113·w − 0.00482·w², at least 45 m
//   in-run  the in-run's width, 2.5 m
namespace norm {
inline constexpr double chord = 1.005;
inline constexpr std::array<double, 2> landing{0.68, 12.44};
inline constexpr double radius = 380;
inline constexpr double round = 10;
inline constexpr std::array<double, 2> p{0.5, 2.5};
inline constexpr double foot = 6;
inline constexpr double l = 1.4;
inline constexpr std::array<double, 2> r1{0.14, 0.95};
inline constexpr std::array<double, 2> t{0.25, 0.95};
inline constexpr std::array<double, 2> s{0.025, 0.7};
inline constexpr std::array<double, 3> e1{92.3, 1.517, 0.426};
inline constexpr std::array<double, 3> e2{67.3, 0.944, 0.331};
inline constexpr std::array<double, 4> a{18.9, 1.113, 0.00482, 45};
inline constexpr double in_run_width = 2.5;
}  // namespace norm

// The start tower's deck, m, and the radius the in-run is rolled over on from it, m (est.); the
// in-run above the top gate, m along it (room for the bar).
inline constexpr double platform = 3;
inline constexpr double roll = 8;
inline constexpr double above = 2;
// THE RAISED IN-RUN: the ground under the deck laid at `slope` degrees up from the knoll's foot
// under the edge and lowered to that where the map stands higher, the cut banked out `bank` m
// across a metre deep; the deck the surface `half` m either side of the line.
namespace raised {
inline constexpr double slope = 8;
inline constexpr double bank = 1.5;
inline constexpr double half = 2.8;
}  // namespace raised
// The most height between two gates, m (the norm's 0.40).
inline constexpr double gate_height = 0.4;
// The jumper the in-run's speeds are worked for, a point mass from the gate: his mass with his
// kit, kg; his crouch's drag area, m²; the air's density, kg/m³; the ice tracks' friction.
namespace skier {
inline constexpr double mass = 67.5;
inline constexpr double crouch = 0.15;
inline constexpr double air = 1.24;
inline constexpr double friction = 0.007;
}  // namespace skier
// The fall line, m of plan past U, and the level out-run past the norm's length, m.
inline constexpr double fall_line = 20;
inline constexpr double past = 15;
// The hill's width, m, how far past its edges its grading eases out, m, and the trees cleared
// within this of them, m.
inline constexpr double width = 34;
inline constexpr double ease = 14;
inline constexpr double margin = 12;
inline constexpr Search search{24, {-12, -6, 0, 6, 12}, 8, 2, 120, 60};
inline constexpr Fit fit{0.3, 40, 160};
inline constexpr Arena arena{40, 40, 45};

}  // namespace ski_jump

}  // namespace ge::mountain::mapgen::venue_rules
