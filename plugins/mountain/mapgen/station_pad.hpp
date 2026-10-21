// R26 — THE PAD a top station stands on, and a chair's UNLOAD RAMP on it.
//
// A lift's top is an area cut and filled into the slope: on a peak cut more than filled, so its
// downhill edge is a LIP; its DECK along the line is level and LEANS off to both sides, so a
// rider stood off the chair slides toward whichever run he picks. A chair's pad carries its
// unload ramp; the ground under the last stretch of the line is cut below the rope's way in
// (the APPROACH). Pressed into the ground before the runs are walked, pressed again over them
// once they stand, and groomed once the crust is folded in. Pure over the plan: draws nothing.
// Held by tests/mountain_resort_build_test.cpp (the pads, and the ground after them).
//
// ge:summary R26 the station pads: pressed, levelled again, groomed; a chair's unload ramp
// ge:keywords station-pad pad pad-shape press-pads relevel-pads groom-pads pad-surface drag-top
// ge:keywords drag-tops-of unload-point cut-approach
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

struct LiftPlan;

// What a pad is cut to: its radius, m; the half-width of its level deck either side of the
// line, m; how fast it falls off the deck to its rim, m per m.
struct PadShape {
  double r;
  double deck;
  double lean;
};

// A top station's pad: its middle (the top of the line), the level of its deck, its shape, the
// way up the line (a unit vector), and on a chair the unload point.
struct StationPad {
  double r;
  double deck;
  double lean;
  std::string lift;
  LiftKind kind;
  double x;
  double z;
  double y;
  double dx;
  double dz;
  double length;  // the line's plan length, m
  std::optional<PlanPoint> unload;
};

// Whether (x, z) stands on a run's snow, as the pressing and the grooming keep off it.
using OnRun = std::function<bool(double x, double z)>;
using HeightAt = std::function<double(double x, double z)>;

// The pad R26 cuts.
PadShape pad_shape();

// The pad's own surface at (x, z): the deck's level, less its lean past the deck.
double pad_surface(const StationPad& p, double x, double z);

// The lifts whose tops stand on a pad: every gondola and chair.
inline bool padded(LiftKind kind) { return kind != LiftKind::drag; }

// A DRAG'S TOP as the ramps off it read it (R26): the ground a rider is let go on.
StationPad drag_top(const LiftPlan& l, const HeightAt& height);

// Every drag's top a run leaves (`drag_tops_of`); `froms` are the lifts the runs leave.
std::vector<StationPad> drag_tops_of(std::span<const LiftPlan> lifts,
                                     std::span<const std::string> froms, const HeightAt& height);

// Where a chair's rider stands up: `lift.unload.at` metres short of its top, down its line.
PlanPoint unload_point(PlanPoint bottom, PlanPoint top);

// R26 — press every padded top's pad (and a chair's ramp) into `ground`, cut to `shape`.
std::vector<StationPad> press_pads(ge::core::Heightfield& ground, std::span<const LiftPlan> lifts,
                                   const PadShape& shape);

// R26 — press the pads again at the levels they were cut to, everywhere but on a run.
void relevel_pads(ge::core::Heightfield& ground, std::span<const StationPad> pads,
                  const OnRun& on_run);

// R26 — groom every pad and the ground eased into it, never over a run's snow.
void groom_pads(std::span<const StationPad> pads, ge::core::Heightfield& packed,
                const OnRun& on_run);

}  // namespace ge::mountain::mapgen
