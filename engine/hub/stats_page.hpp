// THE HUB'S PLAYER, MATCH AND LEADERS PAGES (docs/accounts.md § The hub's pages): the match reports
// and the totals a master keeps (account/matches.hpp), read by players in a browser and by tools as
// JSON. They are served only by a hub that runs the account service (`--accounts=<dir>`), which
// holds the reports in its own process; a hub without one answers each path with a page (or a
// JSON error) saying it keeps no player stats.
//
//   /player/{name}     an account: the totals, by game and mode, the weapons, the game's other
//                      columns, the recent matches; filtered by game, map, mode, the modes left
//                      out (`without`, any number) and the ping (`ping=80`: matches under 80 ms)
//   /match/{id}        one kept report (the id in 16 hex digits): the teams, the scoreboard, each
//                      player's weapons and columns, a link to its demo when one was recorded
//   /leaders           the best accounts by one stat (`stat=`: frags, wins, matches, score,
//                      efficiency, kd, frags_per_match, a column, or `<weapon>_acc`), under the
//                      Player page's filters, all time or over the last `days` (the kept reports)
//   /v1/players/{name}, /v1/matches?game=&mode=&player=&page=, /v1/matches/{id},
//   /v1/leaders?game=&mode=&stat=&days=   the same as JSON
//
// THE GAME'S COLUMNS. Columns named `<w>_attacks`, `<w>_hits`, `<w>_kills` and `<w>_deaths` are a
// weapon's, shown as one row with its accuracy (hits over attacks), the weapons ordered by kills;
// every other column is shown by its name, a `_max` one as the best of a match. A ratio among the
// leaders (efficiency, kd, frags a match, an accuracy) counts only an account with at least
// kLeadersMinMatches matches under the filter.
//
// ge:summary The hub's Player, Match and Leaders pages and their JSON, over a master's reports
// ge:keywords hub player-page match-page leaders player-stats weapon-accuracy stats_answer
// ge:keywords v1-players v1-matches v1-leaders totals-filter ping-filter without-mode
// ge:skill net
// ge:test tests/hub_stats_page_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "account/service.hpp"
#include "hub/api.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

constexpr int kLeadersMinMatches = 3;  // a ratio's leaders played at least this many matches
constexpr int kLeadersShown = 50;
constexpr int kRecentShown = 20;  // a Player page's recent matches, and a /v1/matches page

// Text made safe in a URL's path or query: every byte but letters, digits and -._~ as %XX.
std::string url_text(std::string_view s);
// True when `path` (without its query) is one of the paths above.
bool stats_path(std::string_view path);
// The answer to GET `target` (a path with its query) when its path is one of the above; false
// otherwise. `svc` null: this hub keeps no player stats. `unix_s` dates `days`.
bool stats_answer(const HubSpec& spec, const account::Service* svc, std::string_view target,
                  int64_t unix_s, Answer& out);

}  // namespace ge::hub
