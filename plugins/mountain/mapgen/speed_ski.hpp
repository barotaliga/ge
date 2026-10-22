// R34 — A SPEED-SKIING TRACK SET OVER A BUILT MAP. Not a course on the piste: a track of its own,
// cut STRAIGHT down the fall line of the face, a strip of the mountain closed off, graded smooth
// and groomed hard, with a safety margin cleared either side of it. Down it the racer is launched
// from a start platform near the top, TIMED through a 100 m zone, and stopped on a run-out whose
// slope eases off into the valley; his result is his speed between the zone's two lines.
//
// THE LINE IS SEARCHED: every column of the face and a few bearings either side of the fall line,
// each read down the face as the ground across the track's width, taken to its mean along it, its
// crests rounded so nothing at 200 km/h leaves the snow and never rising (the profile the
// snowcats would grade), held within a few metres of the ground it is cut from. Down each, every
// start is skied by a point mass with the top class's numbers (`speed_ski::stop`: tucked to the
// trap, then stood up into the wind and skidding once slow): the line whose final comes through
// the trap nearest the speed this map's track is built for (dealt inside the top class's band off
// a stream of the map's seed and a salt), and whose run-out stops that racer before the map ends,
// is the track. A QUALIFICATION starts lower down the same track, where the launch gives
// `speed_ski::qualify` km/h less. Everything is a pure function of the map.
//
// ge:summary R34 a speed-skiing track cut straight down a built map's face: launch, zone, run-out
// ge:keywords speed-skiing set-speed-ski speed-track timing-zone launch run-out braking-line
// ge:keywords graded-profile convex-hull crest knee qualification final r34
// ge:skill mountain
// ge:test tests/mountain_speed_ski_test.cpp
#pragma once

#include <optional>
#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// R34 — A SPEED TRACK SET OVER `level`, for run `run` (the qualification, 1, or the final, 2):
// the track cut down the face as the map's own `track`, its ground graded and groomed, its margin
// and finish enclosure cleared, its checkpoints the run's start gate and the timing zone's two
// lines, its spawn and one grid slot the run's start house. A map already carrying this run is
// that map; one carrying the other run's track gives this run on the same track; one carrying any
// other course is set over the map under it. The track keeps the day and the sky of the map it
// was handed.
Level set_speed_ski(const Level& level, int run = 2);

// The speed, km/h, the rule's point mass of the top class carries through the timing zone of
// `level`'s track on its run: what the setter aimed its line at. 0 on a map with no speed track.
double speed_ski_aim(const Level& level);

// THE RUN-OUT'S LINES on a speed track, for a drawing to stand on: the BRAKING LINE
// `speed_ski::run_out::brake` m past the timing zone (no skid and no turn before it) and the
// FINISH ENCLOSURE'S line at the stop. Neither is a gate the run owes. Nothing on a map with no
// speed track.
struct SpeedSkiLines {
  Checkpoint braking;
  Checkpoint finish;
};
std::optional<SpeedSkiLines> speed_ski_lines(const Level& level);

// The profile `y` (samples `step` m apart) under (`below`) or over itself whose curvature is held
// to `c` the other way: the lower convex hull of y + c·x²/2 less the parabola again (no crest
// sharper than 1/c, only ever cut), or the upper concave hull of y − c·x²/2 plus it (no knee
// sharper, only ever filled). In place.
void hull_of(std::vector<double>& y, double step, double c, bool below);

}  // namespace ge::mountain::mapgen
