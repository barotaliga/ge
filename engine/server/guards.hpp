// A SERVER'S GUARDS ON ITS PLAYERS' CHAT: the flood rule every multiplayer game's server applies to
// a player's chat lines, the same for every game on the engine (docs/servers.md § The settings).
//
// THE RULE (`sv_floodprot "N S T"`): a player may send N lines in any S seconds; the line that
// would be the (N+1)-th inside S seconds is dropped and the player is silenced for T seconds.
// The window SLIDES: a new line is checked against the time of the N-th most recent line the
// player was allowed, not against fixed buckets, so a fresh player always has N lines in hand.
// While silenced every line is dropped and the player is told how long is left; talking while
// silenced neither extends the silence nor counts. N is 1..kMostFloodLines, S and T at least 1.
//
// WHAT A PLAYER IS TOLD (`flood_text`): on the line that trips the rule, `sv_floodprotmsg` when
// one is set, else "You can't talk for T seconds."; on a line while silenced, "You can't talk for
// N more seconds" (the time left, whole seconds, rounded down). Nobody else is told.
//
// The clock is the caller's (the server's own time, s): a guard never reads a wall clock, so a
// session over the simulated network replays to the bit. A guard is a few doubles a seat, made
// with the seat and cleared when the seat is freed; nothing allocates.
//
// ge:summary A server's chat flood rule: N lines in S seconds, else silent for T; the words
// ge:keywords flood-protection floodprot sv_floodprot sv_floodprotmsg chat-flood silence mute
// ge:keywords flood_rule parse_flood_rule flood_check flood_text sliding-window say say_team
// ge:skill net
// ge:test tests/server_guards_test.cpp
#pragma once

#include <string>
#include <string_view>

namespace ge::server {

constexpr int kMostFloodLines = 10;  // N's largest: the ring each guard keeps

struct FloodRule {
  int lines = 10;        // N
  double window_s = 1;   // S
  double silence_s = 1;  // T
};

// "N S T" (three whole numbers: N 1..10, S and T at least 1) read into `rule`. False with
// `error` naming the value, what is wrong and the next action; `rule` is then left alone.
bool parse_flood_rule(std::string_view text, FloodRule& rule, std::string& error);
// The rule as its setting's text: "10 1 1".
std::string flood_rule_text(const FloodRule& rule);

// One player's guard: the times of its last allowed lines (a ring, -1 never) and when its
// silence ends.
struct FloodGuard {
  double times[kMostFloodLines] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  int next = 0;              // the ring's slot the next allowed line is written to
  double silent_until = -1;  // the server time the silence ends (before it: silenced)
};

enum class FloodVerdict : unsigned char {
  allowed,   // the line goes out (and is counted)
  tripped,   // this line broke the rule: dropped, the silence starts now
  silenced,  // a silence was running: dropped
};

// The verdict on a line sent at `now_s` (the server's time, s), the guard updated.
FloodVerdict flood_check(FloodGuard& guard, const FloodRule& rule, double now_s);

// What the player is told for `verdict` (empty for `allowed`): `message` (sv_floodprotmsg) or the
// silence's length on a trip, the time left while silenced. Ends in a newline.
std::string flood_text(FloodVerdict verdict, const FloodGuard& guard, const FloodRule& rule,
                       std::string_view message, double now_s);

}  // namespace ge::server
