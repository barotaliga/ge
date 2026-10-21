// R32, R33, R36 — THE SPEED EVENTS SET OVER A BUILT MAP: the downhill, the super-G and the giant
// slalom. Each is raced on a ski area's course (R28): the downhill on the one with the most
// vertical (in its band where the area has one), from its start gate to the village; the super-G
// and the giant slalom on the course with the most vertical, from a START LOWERED down it into
// their band. What each sets over the map is its course (the start house, the gates, the racing
// line, the jumps, the nets, a speed trap) and what the organisers prepare for it
// (course_prep.hpp, with the crests shaved round).
//
// A DOWNHILL's line comes first and its gates MARK it: the line down the piste that bends the
// least, a speed gate centred on it about every eighty metres, none on a jump. A SUPER-G's and a
// GIANT SLALOM's gates TURN the racer: that line SWUNG either side of itself in turn, a gate's
// side a gate, the swing dealt off a stream of the course's own (the map's seed, a salt, and a
// giant slalom's run), eased from one gate's apex to the next, set straighter before a jump; each
// gate's turning pole stands `pass` metres inside the line's apex. Everything is a pure function
// of the map (and the run), drawing from no other stream.
//
// ge:summary R32 R33 R36 the downhill, super-G and giant slalom set over a built map
// ge:keywords downhill super-g giant-slalom set-downhill set-super-g set-giant-slalom speed-event
// ge:keywords turn-course lowered-start most-vertical-course speed-gate swing crests jumps
// ge:keywords r32 r33 r36
// ge:skill mountain
// ge:test tests/mountain_speed_events_test.cpp
#pragma once

#include <optional>
#include <string>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// R32 — THE COURSE A DOWNHILL IS RACED ON: the id of the ski area's course with the most vertical
// (the one inside the rule's band first, where the area has one); nothing on a map that is not a
// ski area.
std::optional<std::string> downhill_course_of(const Level& level);

// R33, R36 — THE COURSE A TURNING RACE IS RACED ON: the id of the ski area's course with the most
// vertical; nothing on a map that is not a ski area.
std::optional<std::string> most_vertical_course_of(const Level& level);

// R32 — A DOWNHILL SET OVER `level`: its whole piste from its start gate as a downhill course, as
// a map whose checkpoints are its gates and whose spawn and one grid slot are the start house.
// Set over a map carrying another course, it is set over the map under it; one already carrying a
// downhill is that map. The course keeps the day and the sky of the map it was handed.
Level set_downhill(const Level& level);

// R33 — WHERE A SUPER-G STARTS on `level`'s piste, and the super-G set over it (as `set_downhill`).
double super_g_start(const Level& level);
Level set_super_g(const Level& level);

// R36 — WHERE A GIANT SLALOM STARTS, and run `run`'s course (1 or 2) set over the map (as
// `set_downhill`; a map carrying the other run's course is set over the map under it).
double giant_slalom_start(const Level& level);
Level set_giant_slalom(const Level& level, int run = 1);

}  // namespace ge::mountain::mapgen
