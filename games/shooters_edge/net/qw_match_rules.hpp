// THE LOBBY'S MATCH RULES ON THE QUAKEWORLD SERVER: overtime, what the warmup lets a player's moves
// do, and the match's lock against newcomers.
//
// The rules themselves are every classic server's (server/match_rules.hpp); this hands them the
// QuakeWorld tally's facts.
//
// OVERTIME: the engine's overtime (engine/server/overtime.hpp) played over
// the mod's own clock. A lobby's match only (`sv_lobby`), deathmatch only, and never under a mod
// folder's own program, which keeps its own overtime.
//
// Each frame of the match, after the tally (qw_report.hpp): a death the tally counted is
// handed to `overtime_died`; one frame before the mod's clock would run out (`timelimit` minutes
// of the rules' time) on a score the overtime keeps going, the time limit is raised by
// `sv_overtime_minutes` (extra time) or set to 0 (sudden death, the tie-break, the golden frag);
// when the overtime is over the time limit is set to the time already played, so the mod ends
// the match on its next frame in its own words. The change is to this map's rules alone: the next
// map is made from the server's settings again.
//
// PREWAR (`prewar_buttons`): fire and jump stripped from a move as the lobby holds them
// (engine/server/match.hpp, sv_prewar), the player told once a second in warmup.
//
// THE LOCK (`lock_refusal`): the line a newcomer reads when the match is closed to him
// (sv_lockjoin); he joins as a spectator.
//
// ge:summary The QuakeWorld server's match rules: overtime over the mod's clock, prewar, the lock
// ge:keywords overtime sv_overtime sv_overtime_minutes sudden-death golden-frag tie-break
// ge:keywords extra-time qw-overtime overtime_frame sv_prewar prewar_buttons sv_lockjoin
// ge:keywords lock_refusal
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "server/overtime.hpp"
#include "shooters_edge/net/qw_report.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/server/match_rules.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

// The match rules every classic server shares (server/match_rules.hpp).
using shooters_edge::server::lobby_rules;
using shooters_edge::server::lock_refusal;
using shooters_edge::server::OvertimeRun;
using shooters_edge::server::prewar_buttons;

// The score the tally holds now: its players, those who left included.
ge::server::MatchScore tally_score(const MatchTally& t, bool teams);

// One frame of a match (`in_match` false: a new match ahead, everything forgotten) over the
// tally's score and deaths, its lines printed to everyone on `to` (null: nobody).
void overtime_frame(OvertimeRun& run, const shooters_edge::server::RulesRunner& rules,
                    const MatchTally& t, QwServer* to, bool in_match);

}  // namespace shooters_edge::net
