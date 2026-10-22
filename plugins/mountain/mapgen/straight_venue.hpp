// R37 — A VENUE BUILT STRAIGHT DOWN THE FACE: what every freestyle venue cut on a line of its own
// shares (the ski jumping hill today, R43): a PROFILE drawn against the horizontal, the same on
// every map; the LINE searched down the face where that profile cuts and fills least, clear of
// the lift stations and the village; and the ground GRADED to it across the venue's width,
// groomed, its woods and its finish arena cleared and the piste's kickers, drops and cliffs in
// it taken out. Under a stretch that stands on a STRUCTURE (a raised in-run) the ground is
// lowered to the ground the structure stands over, never raised to it, the cut banked out the
// wider the deeper it is.
//
// Everything is a pure function of the map, drawing nothing from any stream.
//
// ge:summary R37 the straight venue: the profile, the line searched down the face, the grading
// ge:keywords straight-venue venue-profile find-line grade-venue raised-structure original-of
// ge:keywords venue-surface r37
// ge:skill mountain
// ge:test tests/mountain_ski_jump_test.cpp
#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/mapgen/venue_rules.hpp"

namespace ge::mountain::mapgen {

// A PROFILE: its height every `dx` m of plan from the platform's back, where its finish line
// stands and where it ends, m of plan; and, for a venue whose top stands on a STRUCTURE, the
// ground UNDER it (a height every `dx` as `y` is, never over it), how far the cut that lowers
// the ground there banks out, m across a metre deep, and the structure's own half-width, m.
struct VenueProfile {
  struct Under {
    std::vector<double> y;
    double bank = 0;
    double half = 0;
  };
  double dx = 1;
  std::vector<double> y;
  double finish = 0;
  double end = 0;
  std::optional<Under> under;
};

// What a venue's rule says about where and how wide it is built.
struct VenueRule {
  double width;
  double ease;
  double margin;
  venue_rules::Search search;
  venue_rules::Fit fit;
  venue_rules::Arena arena;
};

// The profile's height `x` m of plan along it, m, read between its samples.
double profile_height_at(const VenueProfile& p, double x);

// A line down the face and how the venue sits on it: its start, its bearing, the height the
// profile is raised by, and what it cost.
struct VenueLine {
  double x;
  double z;
  double heading;
  double shift;
  double cost;
};

// THE VENUE'S LINE on `level`: the column, the bearing and the start where the profile cuts and
// fills least, clear of the stations and the village; nothing on a map too small for it.
std::optional<VenueLine> find_line(const Level& level, const VenueProfile& p, const VenueRule& r);

// The map under any course or venue set over `level` (itself when none is).
std::shared_ptr<const Level> original_of(const Level& level);

// A VENUE GRADED over a map: the line it stands on, its frame (forward and right), and the map
// with its ground graded and groomed, its woods, kickers, drops and cliffs cleared, every course
// on it taken off, and the `track` down it (a station every 2 m of plan, its arc the plan).
struct GradedVenue {
  VenueLine line;
  double fx, fz, rx, rz;
  Level level;
  // The venue's height `d` m of plan along its line, m.
  double y_at(const VenueProfile& p, double d) const {
    return profile_height_at(p, d) + line.shift;
  }
};

// THE VENUE OF PROFILE `p` TO RULE `r` over `original` (a map with no course on it), on the line
// searched for it (the map's middle, straight down, when none fits).
GradedVenue grade_venue(const Level& original, const VenueProfile& p, const VenueRule& r);

}  // namespace ge::mountain::mapgen
