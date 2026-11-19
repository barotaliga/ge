// HOW OFTEN A SERVER ANSWERS A GUESS (`sv_rconrate`, `sv_passwordrate`), the same rules for every
// game's server (docs/servers.md § The settings): a bucket of tries that refills over time, so a
// script guessing the remote console's password or a player's cannot try thousands a second.
//
// - THE VALUE: `limit[/period][*burst]`: `limit` tries a `period` (a number of seconds or `sec`,
//   `min`, `hour`; 1 s unset), at most `burst` at once (5 unset). "0": no limit.
// - THE BUCKET: holds `burst` tokens and gains `limit` every `period`; a try takes one, and a try
//   with no token left is not answered at all. A try that turns out good (the right password) gives
//   its token back, so the server's own administrator is never held up by a guesser's misses
//   beyond the moment the bucket is empty.
// - THE TWO RULES: `sv_rconrate` holds the remote console; `sv_passwordrate` holds connects
//   refused for a wrong player's or spectator's password (only a wrong one takes a try). Each game
//   sets its defaults: the first game's "10*10" (ten a second) and 0, Quake II's 1 and 1.
//
// One bucket per server (not per address: a guesser has many). The clock is the caller's (the
// server's own time, s). The rules are `kLimitRules` (an endpoint's, read when it starts by
// `limit_settings`).
//
// ge:summary sv_rconrate, sv_passwordrate: a refilling bucket of tries, the value's syntax
// ge:keywords sv_rconrate sv_passwordrate rate-limit token-bucket rcon-guess password-guess
// ge:keywords limit_settings limit_take limit_give try-limit
// ge:skill net
// ge:test tests/server_limit_test.cpp
#pragma once

#include <string>

#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kLimitRules[] = {
    {"sv_rconrate", "How often the remote console is answered.", nullptr,
     "0: no limit; N[/period][*burst]: N tries a period (1 s, or sec, min, hour), at most burst "
     "at once (5), a good password's try given back"},
    {"sv_passwordrate", "How often a connect with a wrong password is answered.", nullptr,
     "0: no limit; N[/period][*burst]: N wrong passwords a period (1 s, or sec, min, hour), at "
     "most burst at once (5)"},
};

struct TryLimit {
  double limit = 0;     // tries a period (0: no limit)
  double period_s = 1;  // the period, s
  double burst = 5;     // the most tries at once
};

// "limit[/period][*burst]" onto `out`; false with `error` (naming `name`) when it is not that.
bool parse_limit(const std::string& name, const std::string& value, TryLimit& out,
                 std::string& error);

// Both rules from `c` onto `rcon` and `password` (unset: what they hold, a game's defaults).
bool limit_settings(const ServerConfig& c, TryLimit& rcon, TryLimit& password, std::string& error);

struct TryBucket {
  double tokens = -1;  // -1: full, not yet used
  double at = 0;       // when they were counted, s
};

// Whether a try now is answered; an answered try takes a token.
bool limit_take(TryBucket& b, double now, const TryLimit& rule);
// A try that was good gives its token back.
void limit_give(TryBucket& b, const TryLimit& rule);

}  // namespace ge::server
