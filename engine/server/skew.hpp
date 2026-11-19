// A CLIENT WHOSE CLOCK RUNS FAST (`sv_speedwindow`, `sv_speedwarn`, `sv_speedkick`), the same rule
// for every game's server (docs/servers.md § The settings), beside the speed check that cuts each
// move to the time that passed: over a window of real time, the move time a client's commands
// ask for divided by that time. A game's own clock sped up (a cheat, or a broken machine) asks
// for more than 1.
//
// - THE COUNT: each packet's new commands (the newest, and the older two when packets were lost
//   between, as far as the packet carries them), their milliseconds summed (`skew_packet`).
// - THE CHECK (`skew_check`), once a window has passed: above `sv_speedwarn` the server logs
//   "<name> [<address>]: time skew <ratio>"; above `sv_speedkick` the client is dropped and the
//   others told "<name> was dropped for a fast clock.". Each is off at 0 (the default) and means
//   something only above 1. Then a new window starts.
//
// The clock is the caller's (the server's own time, s): a session over the simulated network
// replays to the bit. The rules are `kSkewRules` (an endpoint's, read when it starts by
// `skew_settings`).
//
// ge:summary sv_speedwarn, sv_speedkick: a client's asked move time against real time per window
// ge:keywords sv_speedwindow sv_speedwarn sv_speedkick time-skew timescale fast-clock speed-cheat
// ge:keywords skew_packet skew_check skew_settings
// ge:skill net
// ge:test tests/server_skew_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kSkewRules[] = {
    {"sv_speedwindow", "The seconds over which a client's move time is held against real time.",
     nullptr, "1..600: that many seconds (16 the default)"},
    {"sv_speedwarn", "The ratio of move time to real time above which a client is logged.", nullptr,
     "0: never (the default); above 1: that ratio, for example 1.05"},
    {"sv_speedkick", "The ratio of move time to real time above which a client is dropped.",
     nullptr, "0: never (the default); above 1: that ratio, for example 1.15"},
};

struct SkewRule {
  double window_s = 16;
  double warn = 0;  // 0 (or 1 and under): off
  double kick = 0;
};

// The three rules from `c` (unset: the defaults). False with `error` naming the value.
bool skew_settings(const ServerConfig& c, SkewRule& out, std::string& error);

// One client's count: the window's start, the move time asked in it, the last packet's sequence.
struct SkewWatch {
  double start = -1;  // -1: no window yet
  double asked_ms = 0;
  uint32_t last_sequence = 0;
};

// A packet's commands (`msec`: the oldest first, the newest last) at sequence `sequence`.
void skew_packet(SkewWatch& w, double now, uint32_t sequence, const int (&msec)[3]);

enum class Skew : uint8_t { fine, warn, kick };
// Once `rule.window_s` has passed since the window's start: what the ratio calls for (into
// `ratio`), and a new window; before then `fine`.
Skew skew_check(SkewWatch& w, double now, const SkewRule& rule, double& ratio);

}  // namespace ge::server
