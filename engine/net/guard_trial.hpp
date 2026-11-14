// A GUARD TRIAL: honest players and cheaters on one server-authoritative world over the simulated
// network, the guard (guard.hpp) judging what arrives. Every player steers a mover with a stick
// read in tens (-100..100 a side) and a heading; the honest ones change their held stick every
// half second, as the replication trial's do. Each cheater plays one cheat for the whole run:
//
// - speed:    the stick pushed past full (127), to move further a tick
// - off-grid: a stick between the tens (37), what no pad of the game sends
// - flood:    three inputs a tick, its clock run fast to move three times as far
// - future:   its view said to be three seconds ahead of what it draws (past any link's delay),
//             to be judged where a target will be
// - teleport: a peer game's own place stated 40 m on, once a second (judged by guard_motion over
//             the places the clients report, as a peer game's host would)
//
// The trial answers whether each cheat is caught (flagged, and out), whether any honest player is
// flagged, and how far each mover went: a caught speed or flood cheater goes no further than an
// honest player at full stick. A pure function of its config: the same trial, the same numbers.
//
// ge:summary A guard trial: honest players and five cheaters over a link, what the guard catches
// ge:keywords net guard anti-cheat trial cheats speed-hack teleport flood future-view off-grid
// ge:skill net
// ge:test tests/net_guard_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/conditions.hpp"
#include "net/guard.hpp"

namespace ge::net {

enum class TrialCheat : uint8_t { none, speed, off_grid, flood, future, teleport };
const char* trial_cheat_name(TrialCheat c);

struct GuardTrialConfig {
  int honest = 8;  // players with no cheat, then one cheater of each kind
  double seconds = 20;
  uint32_t seed = 1;
  int tick_hz = 60;
  LinkConditions link;
};

struct GuardTrialPlayer {
  TrialCheat cheat = TrialCheat::none;
  std::array<uint32_t, static_cast<size_t>(Cheat::count)> counts{};
  double worst = 0;           // the highest score it reached
  bool out = false;           // past the guard's out score
  double out_s = -1;          // when it went out (-1: never)
  double travelled = 0;       // metres its mover went on the server
  double top_speed = 0;       // the fastest second, m/s
  std::vector<double> score;  // its score ten times a second
};

struct GuardTrial {
  std::vector<GuardTrialPlayer> players;  // the honest first, then one a cheat
  std::vector<double> t_s;                // the score samples' times
  double full_stick_mps = 0;              // what full stick moves a mover, m/s
  uint64_t flags = 0;                     // every flag raised
  std::string first_flags;                // the first flag of each kind, a line each
};

GuardTrial run_guard_trial(const GuardTrialConfig& config);

}  // namespace ge::net
