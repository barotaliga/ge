// Overtime on a tie at the time limit (overtime.hpp states the rules).
//
// ge:summary Overtime's score, its decision at the clock's end and at each death
// ge:keywords overtime match_score overtime_time_up overtime_died sudden-death golden-frag
// ge:skill net
// ge:test tests/server_overtime_test.cpp
#include "server/overtime.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace ge::server {

void add_overtime_settings(console::Cvars& cvars) {
  if (cvars.find("sv_overtime") == nullptr)
    cvars.add({.name = "sv_overtime",
               .fallback = kOvertimeRules[0].fallback,
               .flags = console::kServer | console::kServerInfo,
               .help = kOvertimeRules[0].help,
               .kind = console::CvarKind::integer,
               .min = 0,
               .max = 4,
               .values = kOvertimeRules[0].values});
  if (cvars.find("sv_overtime_minutes") == nullptr)
    cvars.add({.name = "sv_overtime_minutes",
               .fallback = kOvertimeRules[1].fallback,
               .flags = console::kServer,
               .help = kOvertimeRules[1].help,
               .kind = console::CvarKind::integer,
               .min = 1,
               .max = 999,
               .values = kOvertimeRules[1].values});
}

OvertimeMode overtime_mode(int value) {
  return value >= 0 && value <= 4 ? static_cast<OvertimeMode>(value) : OvertimeMode::off;
}

MatchScore match_score(const std::vector<ScoredPlayer>& players, bool teams) {
  MatchScore out;
  int present = 0;
  for (const ScoredPlayer& p : players) present += p.present ? 1 : 0;
  if (!teams) {
    std::vector<int64_t> frags;
    for (const ScoredPlayer& p : players)
      if (p.present) frags.push_back(p.frags);
    if (frags.size() < 2) return out;
    std::partial_sort(frags.begin(), frags.begin() + 2, frags.end(), std::greater<>());
    out = {true, frags[0], frags[1]};
    return out;
  }
  if (present < 2) return out;
  std::vector<std::pair<std::string, int64_t>> sums;
  for (const ScoredPlayer& p : players) {
    if (p.team.empty()) continue;
    auto at =
        std::find_if(sums.begin(), sums.end(), [&p](const auto& s) { return s.first == p.team; });
    if (at == sums.end())
      sums.emplace_back(p.team, p.frags);
    else
      at->second += p.frags;
  }
  if (sums.size() != 2) return out;
  out = {true, sums[0].second, sums[1].second};
  return out;
}

int64_t score_gap(const MatchScore& s) {
  if (!s.two) return 0;
  return s.a > s.b ? s.a - s.b : s.b - s.a;
}

TimeUp overtime_time_up(Overtime& o, const OvertimeSettings& s, const MatchScore& score) {
  if (s.mode == OvertimeMode::off || !score.two) return TimeUp::end;
  const int64_t gap = score_gap(score);
  const bool tied = gap == 0;
  const bool goes_on =
      s.mode == OvertimeMode::golden || (s.mode == OvertimeMode::tiebreak ? gap <= 1 : tied);
  // Sudden death, the tie-break and the golden frag already took the clock away: a second end is
  // not theirs to answer, the match ends.
  if (!goes_on || (o.playing != OvertimeMode::off && o.playing != OvertimeMode::extra))
    return TimeUp::end;
  o.said.emplace_back("Time is up and the game is a draw.");
  o.playing = s.mode;
  switch (s.mode) {
    case OvertimeMode::extra: {
      const int m = std::clamp(s.minutes, 1, 999);
      o.periods++;
      o.said.push_back(std::to_string(m) +
                       (m == 1 ? " minute of overtime follows." : " minutes of overtime follow."));
      return TimeUp::extend;
    }
    case OvertimeMode::sudden:
      o.said.emplace_back("Sudden death overtime begins.");
      break;
    case OvertimeMode::tiebreak:
      o.said.emplace_back("Tie-break overtime begins.");
      break;
    case OvertimeMode::golden:
      o.said.emplace_back("Golden frag overtime begins.");
      break;
    case OvertimeMode::off:
      break;
  }
  return TimeUp::no_clock;
}

bool overtime_died(Overtime& o, const OvertimeSettings& s, const MatchScore& score) {
  const int64_t gap = score_gap(score);
  switch (o.playing) {
    case OvertimeMode::sudden:
      return true;
    case OvertimeMode::tiebreak:
      return gap >= (s.short_gap ? 2 : 3);
    case OvertimeMode::golden:
      if (gap > o.gap) return true;
      o.gap = gap;
      return false;
    case OvertimeMode::off:
    case OvertimeMode::extra:
      o.gap = gap;  // noted all match long, for a golden frag's overtime
      return false;
  }
  return false;
}

void overtime_reset(Overtime& o) {
  o.playing = OvertimeMode::off;
  o.periods = 0;
  o.gap = -1;
  o.said.clear();
}

}  // namespace ge::server
