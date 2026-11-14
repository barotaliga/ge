// THE GUARD: the server's checks on what a client sends, so a modified client gains nothing it
// could not have had with its hands (docs/networking.md § Anti-cheat). The server steps the world
// from inputs (replicate.hpp), so a client cannot claim a place or a score; what it can still try
// is an input no hand makes, more inputs than ticks, or a view from the future (to be judged where
// a target will be, M17q). Each is caught on arrival:
//
// - OFF RANGE: a field of the input outside what the game's controls make (a stick past full, a
//   button byte that is no button). The field is clamped into range, and flagged.
// - OFF GRID: a field between the steps the game's controls make (a stick read to 1/100 sent at
//   1/127). Snapped to the nearest step, and flagged.
// - FLOOD: more new inputs over a window than it has ticks, past `flood_slack` (a client running
//   its clock fast to move further). The server applies one a tick whatever comes (the excess
//   waits and is trimmed), so it gains nothing, and the client is flagged.
// - FUTURE: a view tick past the server's own, past `future_slack` ticks (a lag-compensated shot
//   asked at a moment nobody has seen yet). Held to the server's tick, and flagged.
// - MOTION (peer games, where a client states its own place): a step longer than `max_step` is a
//   speed hack or a teleport. `guard_motion` judges two states a tick apart; the caller decides
//   whether to put it back.
//
// Every flag adds its weight to the client's SCORE, which decays by `decay` a second, so an honest
// player's rare slip (a dropped packet's burst of inputs) never builds up; past `out_score` the
// client is OUT (the score is held at twice that, so an out client's line stays readable), which
// the game reads (`guard_out`) to drop it and to report it to the accounts.
// The guard keeps the flags it raised (`flags`, the newest kMaxFlags) for the report and the lab.
// No kernel driver, no scan of the player's machine: only what the server receives is judged.
//
//   guard_init(g, rules)            the game's input layout and limits
//   on each input:  guard_input(g, client, input, tick)          clamps and snaps in place
//   on each message: guard_view(g, client, view_x256, tick)      the view held to the server's
//   every tick:      guard_tick(g, tick)                         the flood window, the decay
//   peer games:      guard_motion(g, client, before, after, tick)
//
// ge:summary Anti-cheat on the server: inputs range- and grid-checked, floods, future views, motion
// ge:keywords net guard anti-cheat cheat speed-hack teleport input-flood off-grid off-range
// ge:keywords future-view lag-compensation score kick ban server-authoritative
// ge:skill net
// ge:test tests/net_guard_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/node.hpp"

namespace ge::net {

enum class Cheat : uint8_t { off_range, off_grid, flood, future, motion, count };
const char* cheat_name(Cheat c);

// One field of a game's input: `bytes` (1 or 2, little-endian) at `offset`, signed or not, its
// values in [min, max] on steps of `step` from `min` (1: every value).
struct InputField {
  uint8_t offset = 0;
  uint8_t bytes = 1;
  bool is_signed = false;
  int32_t min = 0, max = 255;
  int32_t step = 1;
};

struct GuardRules {
  std::vector<InputField> fields;
  int tick_hz = 60;
  double window_s = 2;            // the flood window
  int flood_slack = 12;           // new inputs past the window's ticks before it is a flood
  int future_slack = 2;           // ticks a view may run past the server's (a clock's jitter)
  double max_step = 0;            // motion: the longest step a tick (0: no motion check)
  int motion_fields[2] = {0, 1};  // motion: the state's two plan fields (x, z)
  std::array<double, static_cast<size_t>(Cheat::count)> weight = {1, 1, 10, 2, 8};
  double decay = 1;       // score lost a second
  double out_score = 40;  // past this the client is out
};

struct GuardFlag {
  int client = 0;
  Cheat cheat = Cheat::off_range;
  uint32_t tick = 0;
  int32_t value =
      0;  // what was sent (the field's value, the inputs in the window, the ticks ahead)
};

struct GuardClient {
  std::array<uint32_t, static_cast<size_t>(Cheat::count)> counts{};
  int window_inputs = 0, window_ticks = 0;
  double score = 0, worst = 0;
  bool out = false;
};

struct Guard {
  GuardRules rules;
  std::array<GuardClient, kMaxPeers> clients{};
  std::vector<GuardFlag> flags;  // the newest kMaxFlags, oldest first
  uint64_t raised = 0;           // every flag ever raised
};

constexpr size_t kMaxFlags = 256;

void guard_init(Guard& g, const GuardRules& rules);
// A new input from `client` (input_size bytes): each field clamped into range and snapped onto
// its grid in place. Returns the flags it raised (0: a hand could have made it).
int guard_input(Guard& g, int client, uint8_t* input, uint32_t tick);
// The view a client says it draws (ticks x256), held to the server's `tick` plus the slack.
uint32_t guard_view(Guard& g, int client, uint32_t view_x256, uint32_t tick);
// Every server tick: the flood window closed when due, every score decayed.
void guard_tick(Guard& g, uint32_t tick);
// Peer games: false (and flagged) when `after` is further from `before` than a tick allows.
bool guard_motion(Guard& g, int client, const double* before, const double* after, uint32_t tick);
// A client gone or a new one in its slot: its record cleared.
void guard_reset(Guard& g, int client);
bool guard_out(const Guard& g, int client);
// One line for a log or a report: "flood x3, future x1; score 12.0".
std::string guard_summary(const Guard& g, int client);

}  // namespace ge::net
