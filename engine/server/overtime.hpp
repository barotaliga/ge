// OVERTIME: what a match does when its clock runs out on a tie, for any game with frags and a
// time limit (`sv_overtime`, `sv_overtime_minutes`).
//
// THE SCORE COMPARED (`match_score`): without teams, the frags of the two best players present
// (fewer than two: no overtime); with teams, each team's frags summed over its players, those who
// left included, when there are exactly two teams and more than one player present.
//
// AT THE CLOCK'S END (`overtime_time_up`), by the mode:
// - off: the match ends.
// - extra: on a true tie, `minutes` more go on the clock; when they run out the same question is
//   asked again, so extra time repeats while the score stays tied.
// - sudden: on a true tie the clock is taken away and the match ends at the next death, whoever's.
// - tiebreak: when the gap is at most one, the clock is taken away and the match ends at the
//   first death after which the gap is three (two in a deathmatch-4 style game, `short_gap`).
// - golden: whatever the gap, the clock is taken away; the gap between the two is noted at every
//   death all match long (`overtime_died`), and in overtime the match ends at the first death
//   that leaves the gap wider than the last one noted.
// The frag limit still ends the match in any phase; that is the game's own.
//
// THE WORDS a game prints to everyone are in `said` (cleared by the game), in plain player text:
// "Time is up and the game is a draw.", then "3 minutes of overtime follow." or "Sudden death
// overtime begins." and so on.
//
// A pure function of the scores and the calls in order: no clock, no randomness.
//
// ge:summary Overtime on a tie at the time limit: extra time, sudden death, tie-break, golden frag
// ge:keywords overtime extra-time sudden-death tie-break golden-frag sv_overtime
// ge:keywords sv_overtime_minutes match_score overtime_time_up overtime_died time-limit tie
// ge:skill net
// ge:test tests/server_overtime_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "server/server.hpp"

namespace ge::server {

// The two settings as rules a game's server lists beside its own (ServerGame::rules), so a
// server.cfg, an administrator or a vote may change them for a map.
inline constexpr GameRule kOvertimeRules[] = {
    {"sv_overtime", "What a match does when its time runs out on a tie.", "0",
     "0: it ends a draw (the default); 1: extra time, again while still tied; 2: sudden death, "
     "the next death ends it; 3: a tie-break when one frag or none apart, won by a gap of three; "
     "4: a golden frag, the first death that widens the gap ends it"},
    {"sv_overtime_minutes", "How long each period of extra time lasts, in minutes.", "3",
     "1..999: that many minutes (3 the default)"},
};

enum class OvertimeMode : uint8_t { off, extra, sudden, tiebreak, golden };

struct OvertimeSettings {
  OvertimeMode mode = OvertimeMode::off;  // sv_overtime 0..4
  int minutes = 3;                        // sv_overtime_minutes 1..999, extra time's length
  bool short_gap = false;                 // a tie-break won by two (deathmatch 4), else three
};
// The two settings registered on a match's own console (a game's rules), when not already there.
void add_overtime_settings(console::Cvars& cvars);
// sv_overtime's number as a mode (out of range: off).
OvertimeMode overtime_mode(int value);

// One player as the score sees it: the team (empty without teams), the frags, and whether the
// player is still on the server (a team counts the frags of those who left).
struct ScoredPlayer {
  std::string team;
  int64_t frags = 0;
  bool present = true;
};

// The two sides compared at the clock's end: `two` false when there are not two to compare.
struct MatchScore {
  bool two = false;
  int64_t a = 0, b = 0;
};
MatchScore match_score(const std::vector<ScoredPlayer>& players, bool teams);
int64_t score_gap(const MatchScore& s);  // |a - b|, 0 without two sides

struct Overtime {
  OvertimeMode playing = OvertimeMode::off;  // the overtime under way (off: none)
  int periods = 0;                           // extra time's periods begun
  int64_t gap = -1;                          // the last gap noted at a death (-1: none yet)
  std::vector<std::string> said;             // lines for everyone, in order (the game clears it)
};

// What happens at the clock's end.
enum class TimeUp : uint8_t {
  end,       // the match ends now
  extend,    // `minutes` more on the clock (extra time)
  no_clock,  // the clock is taken away: the match ends at a death (`overtime_died`)
};
TimeUp overtime_time_up(Overtime& o, const OvertimeSettings& s, const MatchScore& score);

// A player died, the scores now `score`: true when the overtime under way is over and the match
// ends now. Called at every death of the match, so the golden frag notes the gap before overtime.
bool overtime_died(Overtime& o, const OvertimeSettings& s, const MatchScore& score);

// A new match: no overtime, no gap noted.
void overtime_reset(Overtime& o);

}  // namespace ge::server
