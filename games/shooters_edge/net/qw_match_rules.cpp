// The lobby's match rules on the QuakeWorld server (qw_match_rules.hpp states them).
//
// ge:summary The QuakeWorld server's overtime frame over its tally's score and deaths
// ge:keywords overtime_frame tally_score sv_overtime timelimit
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#include "shooters_edge/net/qw_match_rules.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;
namespace sv = shooters_edge::server;

gs::MatchScore tally_score(const MatchTally& t, bool teams) {
  std::vector<gs::ScoredPlayer> players;
  for (const TallySeat& s : t.seats)
    if (s.on) players.push_back({s.team, s.frags - s.base, true});
  for (const TallySeat& s : t.left) players.push_back({s.team, s.frags - s.base, false});
  return gs::match_score(players, teams);
}

void overtime_frame(OvertimeRun& run, const sv::RulesRunner& rules, const MatchTally& t,
                    QwServer* to, bool in_match) {
  int64_t deaths = 0;
  for (const TallySeat& s : t.seats) deaths += s.on ? s.deaths : 0;
  for (const TallySeat& s : t.left) deaths += s.deaths;
  const ge::console::Cvar* teamplay = rules ? rules->cvars.find("teamplay") : nullptr;
  std::vector<std::string> said;
  sv::overtime_step(run, rules, tally_score(t, teamplay != nullptr && teamplay->number != 0),
                    deaths, in_match && t.counting, said);
  if (to != nullptr)
    for (const std::string& line : said) qw_print_all(*to, line + "\n");
}

}  // namespace shooters_edge::net
