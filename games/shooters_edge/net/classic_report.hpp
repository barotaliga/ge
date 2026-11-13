// A CLASSIC MATCH'S TALLY AND REPORT (docs/accounts.md § The match report): what each player did
// over a match, read off the rules' world every frame, and the match's report (net/report.hpp)
// built from it when the match ends. The rules keep only the frags; the tally adds the rest by
// watching them: a player whose health falls to nothing died, one whose frags fall on the frame of
// that death killed themself, one whose frags fall while alive killed a team mate. A player who
// leaves before the end stays in the report with the time played.
//
// The tally counts only while `counting` (a lobby's match, or always without a lobby); a frame
// that does not count, a map that changes or a clock that goes back (a restart) starts it again.
// `tally_end` builds the report once and holds the tally until the map changes or restarts. The
// match ends where its limit was reached (the time limit's moment, or the frag that reached the
// frag limit), not when the map changes after the intermission the rules hold the scores up for.
//
//   tally_frame(t, world, counting, accounts)   once a frame, after the rules
//   tally_end(t, world, facts, out) -> false    when nothing was counted
//   classic_mode(teamplay, coop, seats)         "1on1", "ffa", "2on2", "4on4" or "coop"
//
// ge:summary A classic match's tally (frags, deaths, suicides, team kills)
// ge:summary and its report at the end
// ge:keywords match-report match-tally classic-report deaths suicides team-kills match-mode
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_report_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/report.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/server/rules_world.hpp"

namespace shooters_edge::net {

// One player's part so far.
struct TallySeat {
  std::string name, team;
  uint64_t account = 0;
  double since = 0;  // the rules' time the seat started counting, s
  double until = 0;  // the last frame seen, s
  int64_t base = 0;  // the seat's frags when it started counting
  int64_t frags = 0;
  int64_t deaths = 0, suicides = 0, team_kills = 0;
  int64_t ping_sum = 0, ping_n = 0, ping_min = 0, ping_max = 0;  // ms, each frame's ping
  int userid = 0;
  bool on = false, bot = false, alive = false;
};

struct MatchTally {
  std::array<TallySeat, ge::shooter::kMaxClients> seats{};
  std::vector<TallySeat> left;  // players who left before the end
  std::string map;
  double start = 0;   // the rules' time counting started, s
  double last = 0;    // the last frame's, s
  double scored = 0;  // the last frame any player's frags changed, s
  bool counting = false;
  bool ended = false;  // reported: held until the map changes or restarts
};

// What the game knows of the match that the world does not say.
struct TallyFacts {
  std::string mode;                     // classic_mode's
  double fraglimit = 0, timelimit = 0;  // the rules' (timelimit in minutes; 0 none)
  bool teams = false;
  bool stopped = false;  // the map stopped on an error
};

const char* classic_mode(bool teamplay, bool coop, int seats);

void tally_frame(MatchTally& t, const shooters_edge::server::RulesWorld& w, bool counting,
                 const std::array<uint64_t, ge::shooter::kMaxClients>& accounts);

bool tally_end(MatchTally& t, const shooters_edge::server::RulesWorld& w, const TallyFacts& facts,
               ge::net::MatchReport& out);

}  // namespace shooters_edge::net
