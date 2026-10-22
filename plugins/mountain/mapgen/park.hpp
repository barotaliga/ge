// R20 — THE TERRAIN PARK: a run of groomed kickers down the piste raced, laid only on a map built
// for a TRICKS run (`GenerateOptions::tricks`). The mountain, the piste, its start and the course
// are the seed's own; the park changes the line's ground and, on a ski area, where the gates stand.
//
// ON THE PISTE, where a skier already has his speed: groomed, clear of trees, and going on down,
// so a skier who has taken the park comes to the finish without being told where to go. THREE
// SIZES (`trick::low`, `medium`, `high`), laid in turn and round again, so a run down the park is
// a ladder: a lip for a grab, one for a flip, one for a flip with a spin in it. Every one is
// BUILT past its lip: a level deck, a landing slope falling to a floor dug under the piste, and a
// run-out climbing back to the line. The slope meets a skier at the angle he falls at and takes
// the landing into his legs a little at a time.
//
// BUILT AGAINST THE HORIZONTAL, as a park is: the ramp climbs from the altitude of its foot to a
// lip `height` above it, the deck is level at the lip's altitude, the slope falls from it to the
// floor. A kicker shaped in the line's own frame on a falling piste would lose its fall off the
// take-off and have its deck fall away under the flight. The flight does not follow the line, so
// each kicker stands where the line barely turns from the ramp's foot to the landing's, on a
// moderate pitch. STAMPED BY ARC under every cell near the line, at full height across the piste
// and its shoulders, faded into the mountain beyond.
//
// SPACED `trick::gap` of piste apart, and OFF THE GATES (R11): no gate on a ramp, a deck, a
// landing or a run-out (a reset stands a skier at the last gate he took, which must be on the
// line), and the gate before a ramp a run-in back, the line falling `trick::run_in` times the
// lip's height to its foot. Nothing here draws from any stream: asking for the park moves nothing
// the seed drew.
//
// ge:summary R20 the terrain park: planned down a piste, built against the horizontal, stamped
// ge:keywords terrain-park park trick-kicker tricks built-landing deck dig run-out run-in r20
// ge:skill mountain
// ge:test tests/mountain_park_test.cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// What a park's line asks of it besides R20: the gates' arcs it keeps off, the stretches it
// leaves alone (a drift), and which gaps from one run-out to the next ramp's foot it allows: any
// when `slot` is 0, else one in [slot, slot + spread] (one gate between) or past `far` (room for
// the gates' own spacing).
struct ParkAsk {
  std::vector<double> gates;
  std::vector<std::pair<double, double>> avoid;
  double slot = 0;
  double spread = 0;
  double far = 0;
};

// R20 — the park planned down `line` (a finished piste or course): its kickers in the order
// skied, `T1…`, clear of the kickers already on it (`taken`) and the drops across it; or why
// none stands (fewer than `trick::count.min` fit). Nothing is stamped.
struct ParkPlan {
  std::vector<Kicker> field;
  std::string why;
};
ParkPlan plan_park(const Track& line, std::span<const Kicker> taken, std::span<const Cliff> drops,
                   const ParkAsk& ask);

// The surface of park kicker `k` `u` m past its lip, as an ALTITUDE: its ramp's foot at `foot`,
// its landing's floor at `floor`, the line under it at `line`. Only inside the kicker.
double park_altitude(const Kicker& k, double u, double foot, double floor, double line);

// The planned `field` built into `level`: stamped into its ground by arc, the track's and the
// gates' heights read back off it, every trunk the park moved taken off, and the kickers added
// with their lips' heights.
void stamp_park(Level& level, std::vector<Kicker> field);

}  // namespace ge::mountain::mapgen
