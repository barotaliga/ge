// THE CHECKS EVERY MAP SHARES, whatever it was built as (the single piste, or a ski area raced
// down one of its courses): the start line (R13), and the day and the weather (R15, R19). Each
// files its findings through `add` in a fixed order, with fixed text.
// Held by tests/mountain_generate_test.cpp (`findings`) and tests/mountain_resort_level_test.cpp.
//
// ge:summary The checks every map shares: the start line, the day and the weather
// ge:keywords analysis start-line check-start-line check-day day weather
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include "mountain/analysis/index.hpp"

namespace ge::mountain::analysis {

// R13 — the start line: the slots, each on the piste, on packed snow, at its head, facing down it.
void check_start_line(const mapgen::Level& level, const Add& add);

// R15 — the day from the region's bands (and, where the face was turned, the sun within the
// bearing); R19 — a sky the rule deals, at numbers inside its bands.
void check_day(const mapgen::Level& level, const Add& add);

}  // namespace ge::mountain::analysis
