// R26 — THE NEXT LIFT'S QUEUE OFF A TOP: where the gondola tops out beside the peak's chair,
// the rider let go at the top reaches that chair's boarding ring by skiing ON — the ring lies
// AHEAD of him and to his right, on snow that falls to it all the way. A dealt massif's
// half: a real face's aim search is M4d, and grading the leg again once the ramps stand is M4b3.
//
// `chain_bottom` places the chair's bottom station so its ring lands on an aim; `lay_chain`
// takes the first aim whose way asks no more than R26's earthworks and grades the way into the
// mountain; `way_falls` and `keeps_chain` read whether the snow still carries a rider there.
// Pure over its arguments; draws nothing from the stream.
// Held by tests/mountain_resort_build_test.cpp (the lifts, and the ground after the pads).
//
// ge:summary R26 the way off the gondola's top to the peak's chair's queue, laid and graded
// ge:keywords lift-chain chain-bottom chain-way lay-chain way-falls keeps-chain chain-of off-way
// ge:keywords off-queue groom-way earthworks press-way
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/resort.hpp"
#include "mountain/mapgen/station_pad.hpp"
#include "mountain/mapgen/summit_ramps.hpp"

namespace ge::mountain::mapgen {

// The way from a top to the next lift's queue: where the rider is let go, the ring, and the
// next lift's bottom station.
struct ChainWay {
  PlanPoint from;
  PlanPoint ring;
  PlanPoint station;
};

// The bottom station of a `kind` lift climbing to `top` whose boarding ring lies `aim` (a
// bearing off the way the rider faces, rad, to his right; and a reach, m) from where a rider is
// let go at the upper lift's top.
PlanPoint chain_bottom(LiftKind upper_kind, PlanPoint upper_bottom, PlanPoint upper_top,
                       LiftKind kind, PlanPoint top, std::array<double, 2> aim);

// Whether the snow carries a rider down the way to the ring.
bool way_falls(const ChainWay& way, const HeightAt& height);

// How far (x, z) stands off the way's ground, m (its two legs, less `lift.chain.half`).
// The way's two legs as rooms every run walked keeps off (R26): none where the area has no way.
std::vector<RampRoom> chain_rooms(const std::optional<ChainWay>& way);

double off_way(const ChainWay& way, double x, double z);

// How far (x, z) stands off the way's ground as a ramp off the top's deck reads it, m.
double off_queue(const ChainWay& way, double x, double z);

struct LaidChain {
  ChainWay way;
  std::vector<StationPad> pads;
};

// R26 — LAY THE WAY to the peak's chair's queue off the gondola's top, on ground the pads are
// pressed into: the chair's bottom station moved to the first aim whose way asks no more than
// `lift.chain`'s cut and fill (the pads pressed again where it moved), and the way GRADED into
// the mountain. `raw` is the ground before any pad was pressed. Moves the chair in place; none
// where no aim will do.
std::optional<LaidChain> lay_chain(ge::core::Heightfield& ground, std::span<const float> raw,
                                   std::vector<LiftPlan>& lifts, std::vector<StationPad> pads,
                                   const PadShape& shape);

// Whether the peak's chair, where `lifts` stand it, still has its queue ahead of a rider out of
// the gondola on a way the snow carries him down (`snow` false: the snow is not asked).
bool keeps_chain(std::span<const LiftPlan> lifts, const HeightAt& height, bool snow = true);

// The way off the gondola's top to the peak's chair's queue, or none without either.
std::optional<ChainWay> chain_of(std::span<const LiftPlan> lifts);

// The way GROOMED, never over a run's own snow (R26).
void groom_way(const ChainWay& way, ge::core::Heightfield& packed, const OnRun& on_run);

}  // namespace ge::mountain::mapgen
