// R23 — THE PISTE GRADES: how hard a run is, as the colour on its signs, and what building one
// to that colour asks of the mountain under it (the rows, the colour a piste measures as, its
// steepest span, its vertical band, a row scaled to a ski area's run, and the colour a seed
// deals).
//
// A grade is a ROW, like a region, and the generator builds to it. THE UNGRADED ROW is the
// generator before there were grades: every number the rule book's own, drawn in the same
// order off the same stream and no drop laid, so a version-1 map (`single_piste`) is exactly
// the map it was. Its colour is only ever measured, never built to.
// Held by tests/mountain_generate_test.cpp.
//
// ge:summary R23 the piste grades: the ungraded row and the four colours
// ge:keywords grade grades
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <optional>
#include <span>

#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The fall-line shape `mountain.profile` states (R2): a shoulder under the ridge, the peak,
// the ease to the run-out.
struct ProfileShape {
  double shoulder;
  double shoulder_run;
  double ease;
  double runout;
};

// One grade's row: everything R23 lets a colour change. Comments on each block are the
// grade's row.
struct GradeRow {
  std::optional<PisteGrade> id;  // none on the UNGRADED row
  Band steepest;                 // R23: the band the steepest colour window must fall in, m per m
  Band vertical;                 // R2, before the region's share (`vertical_band`)
  ProfileShape profile;
  struct {
    double hills, ridges, rollers;
    struct {
      double count, drop;
    } headwalls;
  } relief;  // R3 multiples, on top of the region's
  struct {
    double steepest;   // the steepest ground the walk runs down
    double max_grade;  // the steepest a grading may leave any grade window
    Band width;
    Band sweeps;
    double pitch;  // how much steeper than the untouched ground the walk reads a pitch (R6)
    Band length;
  } track;
  struct {
    double max_slope, min_slope;
  } spawn;  // R12: the pitch out of the start hut
  struct {
    Band on;
    double height;
    double approach_grade;
    double off;
  } kickers;   // R4, R9
  Band drops;  // R24: drops across the piste (none but on a black)
  struct {
    double count;
    Band beside;
    double clearance;
  } cliffs;    // R22
  Band drift;  // R17: the share of the piste dealt to lie drifted
};

extern const GradeRow kUngraded;

// Every grade's row, in PisteGrade order.
const GradeRow& grade_row(std::optional<PisteGrade> id);

// The colour's name ("green", …): the digest mixes it in, so the spelling is fixed.
const char* grade_name(PisteGrade grade);

// R8, R23 — the colour a piste whose steepest colour window falls at `steepest` m per m is
// signed with: the MEASURED grade.
PisteGrade piste_grade_of(double steepest);

// A row's kicker and drop counts scaled to a run `length` m long: the row's bands are a
// downhill course's, and a ski area's runs are shorter. A run of two kilometres carries the
// whole band, and any run but a green at least one kicker.
GradeRow scaled_row(const GradeRow& row, double length);

// The steepest `track.colour_window` of a piste, m per m, kickers and drops included but a
// terrain park's (none on any map the generator builds yet).
double steepest_span(const Track& track, std::span<const Kicker> kickers);

// R23, R28 — the colour a seed nobody asked a colour of races: dealt off a stream of the seed's
// own at `grade::odds`, so every map of one seed asks for the same colour.
PisteGrade deal_grade(double seed);

// R2, R23 — the band a map's vertical is dealt from.
Band vertical_band(const Region& region, const GradeRow& grade);

}  // namespace ge::mountain::mapgen
