// WHAT A SERVER TELLS A STRANGER WHO ASKS (`sv_status`, `sv_statusrate`, `sv_uptime`), the same
// rules for every game's server (docs/servers.md § The settings): the out-of-band `status` query
// a server browser or a player's `ge` sends.
//
// - WHO IS SHOWN (`sv_status`): 2 (the default) the server's info and a line per player; 1 the
//   info alone; 0 no answer at all (a private server nobody lists).
// - HOW OFTEN (`sv_statusrate`): the answers a period, as `limit[/period][*burst]` (limit.hpp);
//   one bucket for the server, a query past it unanswered. 0: no limit.
// - HOW LONG IT HAS RUN (`sv_uptime`): 1 adds an `uptime` key to the info in its short form
//   (`MM.SS`, `H:MM.SS`, `D+H:MM.SS`), 2 in words ("1 day, 2 hours, 3 mins, 4 secs"); 0 (the
//   default) none.
//
// The clock is the caller's (the server's own time, s, from its start). The rules are
// `kStatusRules` (an endpoint's, read when it starts by `status_settings`); each game keeps its own
// answer's shape and sets its own defaults.
//
// ge:summary sv_status, sv_statusrate, sv_uptime: who a status answer shows, how often, the uptime
// ge:keywords sv_status sv_statusrate sv_uptime status-query server-browser uptime_text
// ge:keywords status_settings status-limit
// ge:skill net
// ge:test tests/server_status_test.cpp
#pragma once

#include <string>

#include "server/limit.hpp"
#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kStatusRules[] = {
    {"sv_status", "What a status query is answered with.", nullptr,
     "0: no answer; 1: the server's info alone; 2: the info and a line per player (the default)"},
    {"sv_statusrate", "How often a status query is answered.", nullptr,
     "0: no limit; N[/period][*burst]: N answers a period (1 s, or sec, min, hour), at most "
     "burst at once (5)"},
    {"sv_uptime", "Whether a status answer tells how long the server has run.", nullptr,
     "0: not told (the default); 1: an uptime key, short (H:MM.SS); 2: in words"},
};

struct StatusRule {
  int show = 2;    // sv_status
  TryLimit rate;   // sv_statusrate (none unless a game sets one)
  int uptime = 0;  // sv_uptime
};

// The three rules from `c` (unset: what `out` holds, a game's defaults); false with `error`.
bool status_settings(const ServerConfig& c, StatusRule& out, std::string& error);

// `seconds` of uptime in `form` (1 short, 2 words; "" for 0).
std::string uptime_text(double seconds, int form);

}  // namespace ge::server
