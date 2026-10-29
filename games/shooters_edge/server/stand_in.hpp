// A STAND-IN BOT: a seeded wanderer that fills a bot slot until Shooter's Edge's real bots land
// (M18r, games/shooters_edge/bots). It is no bot: it knows no map and plans nothing. It exists so
// the server, its slots, its input log and the `se-server` lab can be driven by many players now,
// through the same seam the real bots will use (shooter/cmd_source.hpp), reading the server and
// never writing it.
//
// What it does each command, off its own seeded stream (`core::Rng`, from the run's seed and its
// slot) and what it reads of its own player and the others:
// - runs forward, and turns to a new heading every 0.6..2.4 s, or at once by 90..180 degrees
//   when it has held forward a quarter of a second and moves slower than 60 units/s (a wall);
// - jumps now and then (one command in 200: about one jump in 2.6 s), strafing a moment as it
//   does;
// - when another living player stands within 900 units and 25 degrees of its heading, faces him
//   (yaw and pitch through `core::libm`) and holds attack.
// Its draws come off its own stream, never the server's, so the server stays a pure function of
// the packets it is handed (the input log replays it with no stand-in at all).
//
// ge:summary A stand-in bot: a seeded wanderer filling a bot slot until the real bots (M18r)
// ge:keywords stand-in bot wanderer seeded cmd-source fill-slot placeholder se-server
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#pragma once

#include <cstdint>

#include "core/prng.hpp"
#include "shooter/cmd_source.hpp"

namespace shooters_edge::server {

struct StandIn {
  ge::core::Rng rng{1};    // its own stream (make_stand_in)
  float yaw = 0;           // degrees, the heading it runs on
  int turn_in_ms = 0;      // until the next free turn
  int stuck_ms = 0;        // held forward while slow
  int strafe_ms = 0;       // strafing for this long yet
  int8_t strafe_side = 0;  // -1 left, 1 right
  bool started = false;
};

// A stand-in for `slot` off the run's `seed`.
StandIn make_stand_in(uint32_t seed, int slot);
// The seam for a stand-in (kept alive and unmoved while the slot is filled).
ge::shooter::CmdSource stand_in_source(StandIn& s);

}  // namespace shooters_edge::server
