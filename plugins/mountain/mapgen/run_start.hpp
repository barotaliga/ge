// R12, R27 — WHERE A RUN OFF A TOP STARTS: out along the face from its top station, on the side
// it leans to and then the other, the first spot a start of its colour can be raced off — slid
// down the fall line until it lies under the station by a GLIDE's fall over the way to it, so a
// skier stood off any lift slides down to his run gathering speed and never climbs to it, and
// near enough that a ramp comes down off the top to it (R26).
//
// Held by tests/mountain_resort_build_test.cpp (each walked run's start).
//
// ge:summary R12 R27 a run's start under its top: on the contour, slid down to a glide's fall
// ge:keywords run-start place-start start-top head-on-contour fair-heading glide siblings
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <span>

#include "core/heightfield.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/resort.hpp"

namespace ge::mountain::mapgen {

// A run's top as its start is placed under it: where it stands, its snow's height, the radius
// of the ground a rider is let go on (a pad's rim; none at a drag's let-go), and the glide a
// ramp off it falls at the least, m per m.
struct StartTop {
  double x, z, y, rim, glide;
};

// A lift's top as the starts under it read it, off `ground`.
StartTop start_top(const ge::core::Heightfield& ground, const LiftPlan& lift, double rim,
                   double glide = resort_rules::lift::top::ramp::fall);

// R27 — where on the fall line through (x, z0) a run off `top` starts: the highest spot under
// the top by a glide's fall, within a ramp's reach; none where the face will not come down that
// far.
std::optional<double> head_on_contour(const ge::core::Heightfield& ground, double x, double z0,
                                      const StartTop& top);

// Whether a start at (x, z) leaving on `heading` is clear of everything it must keep off.
using StartClear = std::function<bool(double x, double z, double heading)>;

// R12, R27 — where on the contour by its top station a run starts, and the heading it leaves
// on: the spec moved there, or none within reach. `top` none looks along the line across the
// face at whatever height.
std::optional<RunSpec> place_start(const ge::core::Heightfield& ground, const RunSpec& spec,
                                   double lean, std::span<const double> siblings,
                                   const StartClear& clear, const std::optional<StartTop>& top);

}  // namespace ge::mountain::mapgen
