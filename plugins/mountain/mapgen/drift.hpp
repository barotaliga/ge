// R17 — THE DRIFTS: stretches of the groomed piste the wind has laid fresh snow across.
// Dealt by arc down the finished piste off a stream of their
// own (the attempt's sub-seed, salted), so they only thin the packed field over the line
// already laid and move nothing else; stamped through the corridor's nearest-segment index, so
// a drift covers the piste's full width and eases in and out along the line.
// On a ski area, a run's drifts that the hub's grooming reaches are cut back to the stretches it
// has not groomed, and the run's whole width groomed over what was cut (`trim_drifts`), so a drift
// never lies half-groomed across the hub's floor.
// Held by tests/mountain_generate_test.cpp (`drifts`, `packed`) and
// tests/mountain_resort_build_test.cpp (the drifts after the hub).
//
// ge:summary R17 drifts across the piste: dealt off their own stream, stamped, trimmed by the hub
// ge:keywords drift drifts trim-drifts hub groomed
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// R17 — deal the drifted stretches of a piste `length` m long, clear of the start, the finish,
// every on-piste kicker, every drop and every `keep_off` stretch, a `share` of it (the grade's).
std::vector<Drift> deal_drifts(uint32_t sub, double length, std::span<const Kicker> kickers,
                               Band share, std::span<const Cliff> drops,
                               std::span<const ArcSpan> keep_off = {});

// How deep a drift lies at arc s, 0 (groomed) … 1 (a drift's core).
double drift_at(std::span<const Drift> drifts, double s);

// R17 — thin the packed field under every drift. `near` / `along` are the corridor's index;
// the stations are `step` m apart. Writes `packed` in place.
void stamp_drifts(ge::core::Heightfield& packed, const std::vector<int32_t>& near,
                  const std::vector<float>& along, std::span<const Drift> drifts, double step);

// R17, R29 — cut a run's drifts back to the stretches the hub's grooming has not reached
// (`before`: the packed field as it lay before the hub was groomed), and groom the run's whole
// width over what was cut. `points` is the run's line.
void trim_drifts(std::span<const TrackPoint> points, std::vector<Drift>& drifts,
                 ge::core::Heightfield& packed, std::span<const float> before);

}  // namespace ge::mountain::mapgen
