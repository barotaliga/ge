// QUAKE II'S TIMED POWERUPS AS ONE TABLE: every power that runs for a while once used (the quad,
// invulnerability, the environment suit, the rebreather, and a pack's own beside them) as one row
// that the bar, the screen tint, the end warning, `use`, the drop on death, picking a dropped one
// up and the shells a player wears all read, in the row's order. A new timed power is a new row
// and a field on the player: nothing else learns its name.
//
// The order is the base game's: where two run at once, the first row held is the one the bar's
// timer, the tint and the warning show. A pack's row goes where its game lists it.
//
// ge:summary Quake II's timed powerups as one table: timer, tint, warning, use, drop, shell
// ge:keywords q2 powerup timed-power quad invulnerability rebreather environment-suit
// ge:keywords dual-fire double-damage power-timer end-warning blend shell drop
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#pragma once

#include <span>

#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

struct SqPower {
  const char* item;              // the item's class name (q2_items.hpp)
  double Q2Player::* until;      // the level time it ends (0 none)
  float tint[4];                 // the screen tint while it runs: red, green, blue, alpha
  const char* warn;              // the sound 3 s before its end
  const char* use_sound;         // the sound of using it (nullptr none)
  int effects = 0;               // the effect bits the player wears while it runs (blinking)
  int render = 0;                // the render bits worn with them
  const char* drop_cvar = nullptr;  // dropped at death in deathmatch when this setting is on
  int drop_dmflag = 0;              // or when `dmflags` has this bit
};

// Every timed power, in the order the game checks them.
std::span<const SqPower> sq_powers();
// The row of item `classname`, or nullptr when it is no timed power.
const SqPower* sq_power(const char* classname);
// The first power `p` holds at level time `now`, or nullptr.
const SqPower* sq_first_power(const Q2Player& p, double now);
// Whether `p` has power `item` running at level time `now`.
bool sq_power_on(const Q2Player& p, const char* item, double now);

}  // namespace shooters_edge::server
