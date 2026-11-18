// OVERTIME ON THE CLASSIC SERVER: the engine's overtime (engine/server/overtime.hpp) played over
// the mod's own clock. A lobby's match only (`sv_lobby`), deathmatch only, and never under a mod
// folder's own program, which keeps its own overtime.
//
// Each frame of the match, after the tally (classic_report.hpp): a death the tally counted is
// handed to `overtime_died`; one frame before the mod's clock would run out (`timelimit` minutes
// of the rules' time) on a score the overtime keeps going, the time limit is raised by
// `sv_overtime_minutes` (extra time) or set to 0 (sudden death, the tie-break, the golden frag);
// when the overtime is over the time limit is set to the time already played, so the mod ends
// the match on its next frame in its own words. The change is to this map's rules alone: the next
// map is made from the server's settings again.
//
// ge:summary Overtime on the classic server: the engine's overtime over the mod's own clock
// ge:keywords overtime sv_overtime sv_overtime_minutes sudden-death golden-frag tie-break
// ge:keywords extra-time classic-overtime overtime_frame
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "server/overtime.hpp"
#include "shooters_edge/net/classic_report.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

struct OvertimeRun {
  ge::server::Overtime overtime;
  int64_t deaths = 0;  // the deaths the tally had counted at the last frame
  bool over = false;   // the overtime's end asked of the mod
};

// The score the tally holds now: its players, those who left included.
ge::server::MatchScore tally_score(const MatchTally& t, bool teams);

// One frame of a match (`in_match` false: a new match ahead, everything forgotten). `frame_s` is
// the rules' step; the lines for everyone are appended to `say`, each with its newline.
void overtime_frame(OvertimeRun& run, const shooters_edge::server::RulesRunner& rules,
                    const MatchTally& t, bool in_match, double frame_s,
                    std::vector<std::string>& say);

}  // namespace shooters_edge::net
