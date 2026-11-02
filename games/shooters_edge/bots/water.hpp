// THE FIGHTER IN THE WATER: its air, the dive it swims and when it gives one up, and the way it
// swims this command. The fighter (fighter.hpp) steers along the answer.
// - AIR: the time its head has been under. Past `kAir_s` a walk along a pool's floor turns up and
//   out by the map's swim field (drowning starts at 12 s).
// - A DIVE (an item in a pocket only the water reaches, swum on the item's own field) is given up
//   when it has been in the water `kDiveLate_s` longer than its swim's planned time (held under a
//   ledge it cannot climb, or lost), or when it is short of air and the way back out is shorter
//   than the way on. That item's dive is then not tried for `kDiveRetry_s` (`best_item` reads
//   `Fighter::dive_off`), and the map's swim field takes it up. Recorded as the `dive` decision.
//
// Deterministic: a pure function of the fighter's state, the server and the time.
//
// ge:summary The fighter in the water: its air, a dive swum or given up, the way to swim
// ge:keywords water swim dive air drowning give-up dive_off dive-wet-s under_since water_way
// ge:keywords swim-field pool-floor stall
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr double kAir_s = 6;  // under this long, the shorter way out of the water (drowning at 12)
constexpr double kDiveMost_s = 8;    // no dive whose swim is longer than this
constexpr double kDiveLate_s = 3;    // a dive this far behind its swim's time is given up
constexpr double kDiveRetry_s = 30;  // and not tried again for this long

// One command's look at the water for player entity `me` at time `t`, `msec` long: the air clock,
// the dive kept or given up (on `why` when non-null). True with `way` the point to swim at when it
// dives or swims out this command. Call every command, alive or dead.
bool water_way(Fighter& f, const server::QcServer& s, int me, double t, int msec, ge::ai::Why* why,
               sh::Vec3f& way);

}  // namespace shooters_edge::bots
