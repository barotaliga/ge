// R17 — THE DRIFTS: stretches of the groomed piste the wind has laid fresh snow across.
// Dealt by arc down the finished piste off a stream of their
// own (the attempt's sub-seed, salted), so they only thin the packed field over the line
// already laid and move nothing else; stamped through the corridor's nearest-segment index, so
// a drift covers the piste's full width and eases in and out along the line.
// Held by tests/mountain_generate_test.cpp (`drifts`, `packed`).
//
// ge:summary R17 drifts across the piste: dealt off their own stream, stamped into the packed field
// ge:keywords drift drifts
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
// every on-piste kicker and every drop, a `share` of it (the grade's).
std::vector<Drift> deal_drifts(uint32_t sub, double length, std::span<const Kicker> kickers,
                               Band share, std::span<const Cliff> drops);

// How deep a drift lies at arc s, 0 (groomed) … 1 (a drift's core).
double drift_at(std::span<const Drift> drifts, double s);

// R17 — thin the packed field under every drift. `near` / `along` are the corridor's index;
// the stations are `step` m apart. Writes `packed` in place.
void stamp_drifts(ge::core::Heightfield& packed, const std::vector<int32_t>& near,
                  const std::vector<float>& along, std::span<const Drift> drifts, double step);

}  // namespace ge::mountain::mapgen
