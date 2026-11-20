// THE FINGER AND THE LEDGER, REPORTED (se-team; docs/goals/M18r.md 18b and 0q): a team match's
// launcher use as its fighters counted it (every command carrying the launcher by what the finger
// did, by sight and by range; the burst held and what it met; the rockets after one lost and at a
// sound; the lob thrown), and each bot's belief of each enemy's body against the server's truth.
//
// ge:summary se-team's report of the finger with the launcher and of the ledger of each enemy
// ge:keywords finger-ledger launcher rockets-per-owned-min burst-by afterfire earfire lob ledger
// ge:keywords foe-body prior-miss se-team
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "lab/lab.hpp"
#include "shooters_edge/net/qw_team.hpp"

namespace shooters_edge::labs {

// The finger's and the ledger's metrics of `r` into `lab`: `rockets` the match's rockets fired,
// `secs` its length.
void report_finger(ge::lab::Lab& lab, const shooters_edge::net::TeamMatchReport& r, int64_t rockets,
                   double secs);

}  // namespace shooters_edge::labs
