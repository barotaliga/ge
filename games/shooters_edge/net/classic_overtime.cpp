// Overtime on the classic server (classic_overtime.hpp states the frame).
//
// ge:summary The classic server's overtime frame: deaths, the clock's end, the mod's time limit
// ge:keywords overtime_frame tally_score sv_overtime timelimit
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#include "shooters_edge/net/classic_overtime.hpp"

#include <algorithm>

#include "core/num_text.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;
namespace sv = shooters_edge::server;

namespace {

double rule(const sv::RulesRunner& rules, const char* name) {
  const ge::console::Cvar* v = rules ? rules->cvars.find(name) : nullptr;
  return v ? v->number : 0;
}

}  // namespace

gs::MatchScore tally_score(const MatchTally& t, bool teams) {
  std::vector<gs::ScoredPlayer> players;
  for (const TallySeat& s : t.seats)
    if (s.on) players.push_back({s.team, s.frags - s.base, true});
  for (const TallySeat& s : t.left) players.push_back({s.team, s.frags - s.base, false});
  return gs::match_score(players, teams);
}

void overtime_frame(OvertimeRun& run, const sv::RulesRunner& rules, const MatchTally& t,
                    bool in_match, double frame_s, std::vector<std::string>& say) {
  int64_t deaths = 0;
  for (const TallySeat& s : t.seats) deaths += s.on ? s.deaths : 0;
  for (const TallySeat& s : t.left) deaths += s.deaths;
  if (!in_match || !t.counting) {
    gs::overtime_reset(run.overtime);
    run.deaths = deaths;
    run.over = false;
    return;
  }
  gs::OvertimeSettings s;
  s.mode = gs::overtime_mode(static_cast<int>(rule(rules, "sv_overtime")));
  s.minutes = static_cast<int>(rule(rules, "sv_overtime_minutes"));
  s.short_gap = rule(rules, "deathmatch") == 4;
  const gs::MatchScore score = tally_score(t, rule(rules, "teamplay") != 0);
  gs::Overtime& o = run.overtime;
  const double played = rules->sv.time;
  bool ends = false;
  if (deaths != run.deaths) {
    run.deaths = deaths;
    ends = gs::overtime_died(o, s, score);
  }
  const double limit = rule(rules, "timelimit");
  const bool clock = o.playing == gs::OvertimeMode::off || o.playing == gs::OvertimeMode::extra;
  if (!ends && !run.over && clock && limit > 0 && played + frame_s >= limit * 60) {
    switch (gs::overtime_time_up(o, s, score)) {
      case gs::TimeUp::end:
        break;  // the mod ends it on its own clock
      case gs::TimeUp::extend:
        rules.console("timelimit " + ge::core::num::to_string(limit + std::clamp(s.minutes, 1, 999)));
        break;
      case gs::TimeUp::no_clock:
        rules.console("timelimit 0");
        break;
    }
  }
  if (ends && !run.over) {
    run.over = true;
    rules.console("timelimit " + ge::core::num::to_string(played / 60));  // run out next frame
  }
  for (std::string& line : o.said) say.push_back(std::move(line) + "\n");
  o.said.clear();
}

}  // namespace shooters_edge::net
