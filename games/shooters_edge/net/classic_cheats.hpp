// THE CLASSIC SERVER'S CHEAT CHECK (docs/networking.md § Anti-cheat): the engine's guard
// (net/guard.hpp) scoring what the classic move already measures, so a player who cheats is
// named in the match report (`flagged`) and a master that trusts the server bans the account
// (docs/accounts.md § Bans).
//
// THE SPEED HACK: a client whose clock runs fast sends commands that claim more time than passed.
// The speed check (shooter/client.hpp) already cuts each command to the time that passed plus a
// bank of at most 500 ms, so the cheat gains nothing; what it cut is counted on the slot
// (`speed_cut_ms`). Once a second the watch reads how much it cut over that second: more than
// `kSpeedSlackMs` (a clock a few percent fast, a burst after a stall the bank did not cover) is a
// `flood` flag carrying the milliseconds cut. A flag weighs 10 and the score decays by 1 a second,
// so a client cut every second for about five seconds is out; one stall's burst never is. Bots
// skip the speed check and are never judged. A slot that empties (or a new client in it) starts
// clean.
//
//   CheatWatch w;                               (or cheat_watch_init(w) to start again)
//   once a frame: cheat_watch_frame(w, sv)      after the frame's packets ran
//   cheat_watch_verdict(w, slot)                "" while clean; the report's `flagged` once out
//
// ge:summary The classic server's cheat check: the speed check's cuts scored by the guard
// ge:keywords anti-cheat speed-hack speed-check guard flagged ban classic-server cheat-watch
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_cheats_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "net/guard.hpp"
#include "shooter/server.hpp"

namespace shooters_edge::net {

constexpr int kSpeedSlackMs = 50;  // ms a second the speed check may cut before it is a flag

// The guard the watch scores with: ticked once a second, its own flood window never filling.
ge::net::Guard cheat_guard();

struct CheatWatch {
  ge::net::Guard guard = cheat_guard();
  std::array<int64_t, ge::shooter::kMaxClients> cut_seen{};  // each slot's `speed_cut_ms` read
  std::array<int64_t, ge::shooter::kMaxClients> cut_window{};
  std::array<bool, ge::shooter::kMaxClients> active{};
  double window_start = -1;  // the server time the second began, s (-1: not yet)
  uint32_t seconds = 0;      // the guard's tick: whole seconds watched
};

void cheat_watch_init(CheatWatch& w);
void cheat_watch_frame(CheatWatch& w, const ge::shooter::Server& sv);
bool cheat_watch_out(const CheatWatch& w, int slot);
std::string cheat_watch_verdict(const CheatWatch& w, int slot);

}  // namespace shooters_edge::net
