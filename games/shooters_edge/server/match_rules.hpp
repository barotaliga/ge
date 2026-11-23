// THE MATCH A CLASSIC SERVER PLAYS, SHARED BY EVERY CLASSIC GAME'S SERVER (QuakeWorld's and Quake
// II's): the lobby's rules on a server's moves and joins, the warmup's hold on the limits,
// overtime over the rules' own clock, the timeout's pause, and the phase in the server's words.
// Each server keeps its own players and network; it hands these its facts (a score, the deaths
// counted, who is calling) and prints what they say.
//
// THE LOBBY'S RULES (`lobby_rules`): sv_prewar, sv_idlekick, sv_teamsize_min and sv_lockjoin copied
// from the server's settings into the lobby's (engine/server/match.hpp).
//
// PREWAR (`prewar_buttons`): fire and jump stripped from a move as the lobby holds them, the player
// told once a second in warmup. A game whose jump is not a button (Quake II's up move) asks
// `prewar_jump_held` and stops the up move itself.
//
// THE LOCK (`lock_refusal`): the line a newcomer reads when the match is closed to him
// (sv_lockjoin); he joins as a spectator.
//
// THE WARMUP'S HOLD (`WarmupHold`): nothing ends a warmup but the lobby, so while it is not the
// match the rules' `fraglimit` and `timelimit` are held at 0 and the configured values kept; a
// value set meanwhile (a vote, the console) becomes the kept one. The match gets them back.
//
// OVERTIME (`overtime_step`): the engine's overtime (engine/server/overtime.hpp) over the rules'
// own clock: a death counted is handed to `overtime_died`; one frame before the clock would run
// out (`timelimit` minutes of the rules' time) on a score the overtime keeps going, the time limit
// is raised by `sv_overtime_minutes` (extra time) or set to 0 (sudden death, the golden frag);
// when the overtime is over the time limit is set to the time already played, so the rules end the
// match on their next frame in their own words. The change is to this map's rules alone.
//
// THE TIMEOUT'S PAUSE (`timeout_hold`): the rules paused while engine/server/timeout.hpp says so.
//
// THE TEAMS' RESULT (`team_result`): who won a team match and by how much, or the tie.
//
// THE PHASE IN THE SERVER'S WORDS (`phase_status`): Standby, Countdown, In progress, End, the
// `status` a match server's info carries (a recording starts on it).
//
// ge:summary The match every classic server plays: prewar, the lock, warmup's hold, overtime, pause
// ge:keywords match-rules overtime sv_overtime sv_overtime_minutes sudden-death golden-frag
// ge:keywords extra-time overtime_step sv_prewar prewar_buttons prewar_jump_held sv_lockjoin
// ge:keywords lock_refusal warmup-hold warmup_hold timeout_hold phase_status lobby_rules
// ge:keywords team_result team-result
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_match_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "server/match.hpp"
#include "server/overtime.hpp"
#include "server/server.hpp"
#include "server/timeout.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

namespace gsv = ge::server;

struct OvertimeRun {
  gsv::Overtime overtime;
  int64_t deaths = 0;  // the deaths counted at the last frame
  bool over = false;   // the overtime's end asked of the rules
};

// One frame of a match: `score` and `deaths` (every death so far, those of players who left too)
// as the game counts them; `in_match` false: a new match ahead, everything forgotten. The lines
// for everyone are appended to `said`.
void overtime_step(OvertimeRun& run, const RulesRunner& rules, const gsv::MatchScore& score,
                   int64_t deaths, bool in_match, std::vector<std::string>& said);

// The server's settings for the lobby's match rules copied into its settings; `teams` the game's
// team rule.
void lobby_rules(gsv::LobbySettings& l, const gsv::ServerConfig& c, bool teams);

// The buttons a move keeps (`held_told`: when this seat was last told, the rules' time). A line
// for the player alone is left in `tell` ("" none). `jump` false: the game's jump is not a button
// and the jump bit is left alone (it asks `prewar_jump_held`).
uint8_t prewar_buttons(const gsv::Lobby& l, int slot, uint8_t buttons, double now,
                       double& held_told, std::string& tell, bool jump = true);
// Whether the lobby holds `slot`'s jump now.
bool prewar_jump_held(const gsv::Lobby& l, int slot);

// "" when `name` may play now, else the line he reads as he is seated as a spectator.
std::string lock_refusal(const gsv::Lobby& l, const std::string& name);

struct WarmupHold {
  bool held = false;
  std::string fraglimit = "0", timelimit = "0";  // the configured values while held
};
// The limits held at 0 while `match` is false, and given back once it is true.
void warmup_hold(WarmupHold& h, const RulesRunner& rules, bool match);

// The rules paused or let go as the timeout says (`held`: paused by it now).
void timeout_hold(const gsv::TimeoutBook& b, RulesRunner& rules, bool& held);

// A team match's end in words: "<team> wins, 12 to 7." or "Tie game, 7 to 7."
std::string team_result(const std::string& a_name, int64_t a, const std::string& b_name, int64_t b);

// The phase as a match server's `status` says it.
const char* phase_status(gsv::LobbyPhase p);

}  // namespace shooters_edge::server
