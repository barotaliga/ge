// A SPEED EVENT'S COURSE: what a downhill (R32), a super-G (R33) and a giant slalom (R36) share
// once set over a built map: the RACING LINE relaxed out of the piste's centreline to bend the
// least inside its edges (`racing_line`), the one reader of a line as set (`speed_line_at`: its
// offset and its signed bend), the SPEED TRAP's place (`trap_arc`) and a downhill's evenly spaced
// gates (`speed_gate_arcs`). Every one is a pure function of the map, drawing from no stream.
//
// ge:summary A speed event's course: the racing line, its reader, the speed trap, the gate arcs
// ge:keywords speed-course racing-line speed-line-at speed-trap trap-arc piste-bend gate-arcs
// ge:keywords downhill super-g giant-slalom line-offset
// ge:skill mountain
// ge:test tests/mountain_speed_events_test.cpp
#pragma once

#include <optional>
#include <vector>

#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The piste's signed bend at arc `s`, 1/m, over `span` m either side: positive turning to the
// skier's right.
double piste_bend_at(const Track& track, double s, double span);

// THE SPEED TRAP's arc: where the piste has fallen the most over the `T.run` metres before it, on
// a straight, late in the course (any stretch where no straight is long enough).
double trap_arc(const Track& track, double from, double to, const TrapRule& T);

// THE RACING LINE from `from` to `to`: how far right of the piste's centreline it stands at each
// station `L.step` m apart, m. It starts and ends on the centreline (the house and the finish)
// and between them bends as little as the piste lets it, held `L.margin` m inside its edges: each
// station drawn to the midpoint of its neighbours a stride either side, coarse strides first so a
// long bend is straightened as one and the short ones last.
std::vector<double> racing_line(const Track& track, double from, double to, const LineRule& L);

// The line's offset `at` m down the piste, off its stations from `from` `step` m apart.
double line_offset(const std::vector<double>& line, double from, double at, double step);

// The arcs of a downhill's gates between the start gate at `from` and the finish at `to`: as near
// `G.target` apart as divides the piste, each stepped off a jump (no gate within `clear` m of
// one) inside the spacing's band.
std::vector<double> speed_gate_arcs(double from, double to, const std::vector<double>& jumps,
                                    const SpacingRule& G, double clear);

// THE RACING LINE `s` m down a speed event's course: how far right of the centreline, m, and how
// sharply it turns there, 1/m, signed (positive to the right), read over 8 m either side. Nothing
// on a map with no speed event set.
struct SpeedLineAt {
  double offset;
  double bend;
};
std::optional<SpeedLineAt> speed_line_at(const Level& level, double s);

}  // namespace ge::mountain::mapgen
