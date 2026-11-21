// THE ITEMS, REPORTED (se-team; docs/goals/M18r.md): a team match's backpacks known and the
// weapon guessed in them, shots and damage by the target's state, the key items lying untaken and
// what the fighters made of it, a launcher lying near a fighter holding none, and pickups by kind a
// player-minute beside people's. Three calls, in the report's order of its metrics.
//
// ge:summary se-team's report of the items: packs known, hits by state, items lying, pickups
// ge:keywords packs-known pack-guess hits-by-state lying-why rl-near took-per-min se-team
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "lab/lab.hpp"
#include "shooters_edge/net/qw_team.hpp"

namespace shooters_edge::labs {

// Every death's pack known, by how its place was known; the weapon guessed in it by the
// guesser's skill; trips to them (`team.packs.*`).
void report_packs(ge::lab::Lab& lab, const shooters_edge::net::TeamMatchReport& r);
// Shots and damage by the target's state (`team.target.*`), the key items lying untaken and why
// (`team.lying.*`), a launcher lying near a fighter holding none (`team.rl-near.*`).
void report_hits(ge::lab::Lab& lab, const shooters_edge::net::TeamMatchReport& r);
// Pickups by kind a player-minute (`team.on-the-way`, `team.took-per-min.*`).
void report_pickups(ge::lab::Lab& lab, const shooters_edge::net::TeamMatchReport& r);

}  // namespace shooters_edge::labs
