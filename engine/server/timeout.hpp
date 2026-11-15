// A TIMEOUT IN THE MATCH: a player calls one (`timeout`), the game stops for everyone for
// `length_s` (`sv_timeoutlength`), and play resumes after a short count (`resume_s`,
// `sv_timeoutresume`), or sooner when the side that called it says `timein`. Each SIDE (a team in a
// team game, else the player himself) has `per_side` (`sv_timeouts`) for the map. The same for
// every game on the engine (docs/servers.md § Timeouts); the game says who the caller is and
// whether its match is on, and stops and starts its world as `timeout_paused` says.
//
// THE WORDS (`timeout_command`): `timeout` calls one, or while one is on says how long is left;
// `timein` (the calling side only) starts the count back to play at once. Anything else is not the
// timeout's (false). Refusals say why and what to do instead. Every line is in `said`, with beside
// it in `to` who it is for (-1 everyone, else that slot), for the game to print and clear.
//
// THE CLOCK is the server's own (it runs on through the pause, as the game's does not). While out,
// the time left is said at 30 and 10 seconds; while resuming, each of the last three seconds; then
// play. `timeout_map_over` ends any timeout and gives every side its timeouts back.
//
// A pure function of the acts in order on the clock given: no wall clock, no randomness.
//
// ge:summary Timeouts in a match: called by a side, the game paused, resumed after a count
// ge:keywords timeout timein pause match-pause break sv_timeouts sv_timeoutlength
// ge:keywords sv_timeoutresume timeout_command timeout_tick timeout_paused per-side
// ge:skill net
// ge:test tests/server_timeout_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::server {

struct TimeoutSettings {
  int per_side = 1;      // sv_timeouts: timeouts each side may call a map (0: none)
  double length_s = 60;  // sv_timeoutlength: how long one stops the game, s
  double resume_s = 5;   // sv_timeoutresume: the count from its end back to play, s
};

enum class TimeoutPhase : uint8_t { none, out, resuming };

// Who says the words: a slot, the name to say, the side (a team's name, or the player's own), and
// whether that slot plays in the match (spectators and bots do not).
struct TimeoutCaller {
  int slot = -1;
  std::string_view name;
  std::string_view side;
  bool player = false;
};

struct TimeoutBook {
  TimeoutSettings settings;
  TimeoutPhase phase = TimeoutPhase::none;
  int caller = -1;
  std::string caller_name, side;
  double ends = 0;  // out: when it runs out; resuming: when play resumes
  int told = -1;    // the whole seconds left said last
  std::vector<std::pair<std::string, int>> used;  // each side's timeouts this map
  std::vector<std::string> said;                  // lines for the game to print, in order
  std::vector<int> to;                            // each line's: -1 everyone, else that slot
  int called = 0;                                 // timeouts called, all maps
};

// Whether the game stays stopped (a timeout is out or counting back to play).
inline bool timeout_paused(const TimeoutBook& b) { return b.phase != TimeoutPhase::none; }
// One of the words above; false when the line is not the timeout's. `in_match`: the match is on.
bool timeout_command(TimeoutBook& b, const TimeoutCaller& who, bool in_match, std::string_view line,
                     double now);
// The time says its lines and moves the timeout on: out to resuming to play.
void timeout_tick(TimeoutBook& b, double now);
// The map is over: any timeout ends and every side has its timeouts again.
void timeout_map_over(TimeoutBook& b);
// The timeouts `side` may still call this map.
int timeout_left(const TimeoutBook& b, std::string_view side);

}  // namespace ge::server
