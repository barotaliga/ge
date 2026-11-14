// FALL LINE'S RUNS ON A HUB (docs/hub.md § Watching in a browser, M56k): a run's input log read
// into the hub archive's row. The log names only its setup (the seed, the rivals, the generator's
// version) and every step's input, so the row is the run played out headless: the map is the
// seed's mountain, the mode the race, the racers the player and his rivals, and each racer's score
// his time at the line in milliseconds (none for one who did not finish). The hub's Watch hands
// the log to Fall Line's web viewer (`--run`), which plays it again on the page.
//
// ge:summary Fall Line's runs on a hub: an input log played out headless into the archive's row
// ge:keywords hub run runs input-log run_row archive race finish-time racers fall_line_hub
// ge:skill net sim
// ge:test tests/fall_line_hub_runs_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "hub/archive.hpp"

namespace fall_line::hub {

// The row of the run `log` (the input log's text): false with the reason in `error` when it is
// another game's, another version's or damaged.
bool run_row(std::span<const uint8_t> log, ge::hub::DemoRow& out, std::string& error);

}  // namespace fall_line::hub
