// THE MATCHES A MASTER KEEPS (docs/accounts.md § The match report): the reports game servers send
// it (net/report.hpp), the last few kept for the hub, and each account's totals per game and mode.
// Every report is kept and shown, but only one from a server whose key the master's owner trusts
// (`--trusted-servers`) adds to any account's totals: a server can say anything about its own
// matches. Totals are kept per game, mode, map and ping band (the player's average ping in the
// match: under 40, 80, 120 or 200 ms, or more), so a player's are read whole, for one map, for one
// mode, with some modes left out, or at a good ping only (`TotalsFilter`); the game's own columns
// (a weapon's hits, the damage given) are summed by name beside the common ones. A report is taken
// once by its server's key and id, so one sent again (its answer lost) counts once. A guest
// (account 0) and a bot add to no totals.
//
// THE LOG. Every report taken is a line appended to `log` (`report <server key> <bytes>`, both in
// hex), which the master writes to its file and clears; reading the file back gives the same
// totals, counted by the trust the master has when it reads them.
//
//   matches_trust(m, key)                     a server's key the owner trusts
//   matches_add(m, server, report) -> Taken   new or again, counted or shown only
//   matches_line(m, line, error)              one log line read back
//   totals_sum(m, account, filter)            an account's totals over what the filter keeps
//
// ge:summary The matches a master keeps: reports by server, totals per account, trusted servers
// ge:keywords match-report totals account-totals trusted-servers matches_add reports-log
// ge:keywords player-stats leaders
// ge:skill net
// ge:test tests/account_matches_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "net/crypto.hpp"
#include "net/report.hpp"

namespace ge::account {

constexpr size_t kMatchesKept = 256;  // the last reports a master keeps for the hub
// The ping bands' upper bounds, ms (the last band is everything past the last bound).
inline constexpr int kPingBands[] = {40, 80, 120, 200};
constexpr int kPingBandCount = 5;
int ping_band(int64_t ping_ms);

// An account's part of the matches it played in one game, mode and map.
struct Totals {
  int64_t matches = 0, wins = 0;  // a win: the best score, or on the best team
  int64_t score = 0, frags = 0, deaths = 0, suicides = 0, team_kills = 0;
  int64_t seconds = 0;
  std::map<std::string, int64_t> columns;  // the game's own, summed by name
  bool operator==(const Totals&) const = default;
};

// Which of an account's totals to sum: "" keeps every game or map; `modes` empty keeps every mode
// but those in `without`; `ping_under` (ms, one of kPingBands) keeps the matches played at an
// average ping under it (0: any).
struct TotalsFilter {
  std::string game{}, map{};
  std::vector<std::string> modes{}, without{};
  int ping_under = 0;
};

// The account, the game, the mode, the map and the ping band.
using TotalsKey = std::tuple<uint64_t, std::string, std::string, std::string, int>;

struct KeptMatch {
  net::Key server{};
  bool counted = false;  // from a trusted server
  net::MatchReport report;
};

struct Matches {
  std::set<net::Key> trusted;
  std::set<std::pair<net::Key, uint64_t>> seen;  // every report taken, by server and id
  std::deque<KeptMatch> kept;                    // the last kMatchesKept, newest last
  std::map<TotalsKey, Totals> totals;
  uint64_t taken = 0, counted = 0, again = 0;
  std::string log;  // the lines since the master last wrote them
};

enum class Taken : uint8_t {
  counted,  // new, from a trusted server: added to the accounts' totals
  shown,    // new, from a server the owner has not listed: kept, added to no totals
  again     // taken before (the server sent it again)
};

void matches_trust(Matches& m, const net::Key& server);
Taken matches_add(Matches& m, const net::Key& server, const net::MatchReport& r);
// A line of the log read back; false with `error` naming what does not read.
bool matches_line(Matches& m, const std::string& line, std::string& error);
// Every game, mode and map the filter keeps, summed (`matches` 0: none).
Totals totals_sum(const Matches& m, uint64_t account, const TotalsFilter& f);

}  // namespace ge::account
