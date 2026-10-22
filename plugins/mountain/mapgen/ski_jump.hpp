// R43 — A SKI JUMPING HILL BUILT OVER A BUILT MAP. As every venue is (R37, straight_venue.hpp), the
// hill is shaped STRAIGHT down the face: the start tower's deck, the IN-RUN (its straight at 35°,
// a transition whose curvature grows evenly to the table's radius, the TABLE to the EDGE), and
// below the edge the LANDING HILL: the knoll's cubic parabola to P, the landing radius from P
// through K to L, the transition onto the level OUT-RUN at U.
//
// THE PROFILE IS DESIGNED OFF THE NORM, then FITTED. Every element is the construction norm's
// formula over the hill's free choices (`hill_of`, `venue_rules::ski_jump::norm`), so the profile
// is the same on every map; then it is dropped onto the line of the face where it cuts and fills
// least. The in-run is laid out along its own arc back from the edge (the norm states it by arc:
// the table's length, the gates' distances e₁ and e₂), the landing hill as heights over the
// horizontal from the edge, and the whole resampled every `kPenStep` m of plan.
//
// THE SURFACE IS ANALYTIC. A table's edge with a 3 m wall under it and a 300 m arc are finer than
// the 2 m grid a map is baked on, so the hill answers `ground_at` and `normal_at` off its own
// profile inside its width (`Level::venue`), the grid cut beneath it; up the raised in-run only
// the deck answers, the ground far under it beside.
//
// Pure, drawing nothing from any stream.
//
// ge:summary R43 the ski jumping hill: the norm's elements, the profile, the gates, the hill set
// ge:keywords ski-jump jumping-hill hill-of in-run table edge knoll landing-hill out-run k-point
// ge:keywords hill-size start-gate design-gate edge-speed jump-distance set-ski-jump r43
// ge:skill mountain
// ge:test tests/mountain_ski_jump_test.cpp
#pragma once

#include <vector>

#include "mountain/mapgen/straight_venue.hpp"
#include "mountain/mapgen/types.hpp"
#include "mountain/mapgen/venue_rules.hpp"

namespace ge::mountain::mapgen {

// THE HILL'S ELEMENTS as the norm derives them from its row (lengths m, speeds m/s, angles rad,
// down positive).
struct HillElements {
  double w, hs;       // K's distance and the hill size
  double h, n;        // the height and the horizontal distance from the edge to K
  double in_run;      // the in-run's angle
  double table;       // the table's
  double slope;       // the landing slope at K
  double slope_p;     // at P
  double slope_foot;  // at the knoll's foot
  double slope_l;     // at L
  double speed;       // the design speed off the edge
  double landing;     // the speed landing at K
  double r_l;         // the landing radius (P to L)
  double r1;          // the transition's radius at the table
  double t;           // the table's length
  double s;           // the table's height over the knoll's foot
  double curve;       // the transition's length from the straight to the table
  double l1, l2;      // the arcs P–K and K–L
  double e1, e2;      // the in-run's length from the top gate and from the lowest
  double a;           // the out-run's level length
};

// The norm's elements for hill `row` (R43).
HillElements hill_of(const venue_rules::ski_jump::Hill& row);

// THE HILL'S PROFILE: its height every `dx` m of plan from the tower's deck (the edge at height
// 0), where its parts stand, m of plan, its gates (the lowest first), and the landing hill's arc
// from the knoll's foot every `dx` of plan past the edge with the scale that makes K read `w`.
struct SkiJumpProfile : VenueProfile {
  JumpHill hill = JumpHill::large;
  HillElements elements{};
  double straight = 0;
  double curve = 0;
  double table = 0;
  double edge = 0;
  double p = 0;
  double k = 0;
  double l = 0;
  double u = 0;
  double outrun = 0;
  std::vector<SkiJumpHill::Gate> gates;
  std::vector<double> arc;
  double scale = 1;
};

// THE HILL (R43) as designed: the same on every map, built once a hill.
const SkiJumpProfile& ski_jump_profile(JumpHill hill);

// The row of the rule a hill is built to.
const venue_rules::ski_jump::Hill& hill_row(JumpHill hill);

// THE DISTANCE a landing `x` m of plan along the hill is credited, m: along the landing profile
// from the knoll's foot, scaled so K reads w; 0 above the edge. Unrounded.
double jump_distance_at(const SkiJumpProfile& p, double x);

// A distance as the measurers mark it: down to the half metre, m.
double measured_distance(double d);

// THE SPEED AT THE EDGE from gate `gate` (an index into `p.gates`, the lowest 0) in calm air: the
// rule's jumper as a point mass down the in-run's arc (the drop, the tracks' friction on his
// weight and the curve's load, his crouch's drag), m/s. A head wind of `head` m/s along the in-run
// adds to the air past him.
double edge_speed(const SkiJumpProfile& p, int gate, double head = 0);

// THE DESIGN GATE: the lowest gate whose jumper reaches the edge at the hill's design speed in
// calm air (an index into `p.gates`).
int design_gate(const SkiJumpProfile& p);

// R43 — A SKI JUMPING HILL BUILT OVER `level`: the hill shaped down the face as the map's own
// `track`, its checkpoints the start gate (the top gate) and the finish line (the fall line), its
// spawn and one grid slot on the top gate, its take-off the map's one on-track kicker (`SJ`), its
// surface the map's ground inside its width (`Level::venue`). A map already carrying this hill is
// that map; one carrying any other course or hill is set over the map under it. The hill keeps
// the day and the sky of the map it was handed.
Level set_ski_jump(const Level& level, JumpHill hill = JumpHill::large);

// The hill's height `d` m of plan along its line on a map carrying one, m (its analytic surface).
double ski_jump_height_at(const Level& level, double d);

}  // namespace ge::mountain::mapgen
