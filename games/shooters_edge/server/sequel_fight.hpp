// The sequel runner's fighting half (sequel_server.hpp states the rules): the weapons' models and
// sounds listed at the map's start, the hand stepped and its shots made, missiles moved, damage
// dealt, deaths and obituaries, the weapon chosen, and the world's rows for the shots in flight.
// Only sequel_server.cpp calls these.
//
// ge:summary The sequel runner's fight: shots, missiles, damage, deaths, the chosen weapon
// ge:keywords sequel fight shots hitscan missiles radius-damage obituary death frags use-weapon
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_fight_test.cpp
#pragma once

#include <string>

#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

// The weapons' view models and the missiles' models into the model list, their sounds into the
// sound list (before the player's model, which stays last).
void fight_precache(SequelServer& s);
// A fresh hand: the blaster, activating; the missiles of the map cleared (`all`).
void fight_spawn(SequelServer& s, int slot);
void fight_clear(SequelServer& s);

// The hand's step for this rules frame, with `attack` held: its shots made.
void fight_step(SequelServer& s, int slot, bool attack);
// The missiles moved a rules frame (0.1 s): touches, fuses, explosions.
void fight_missiles(SequelServer& s);

// Weapon `w` chosen by a number key or `use`; ignored when not held.
void fight_choose(SequelServer& s, int slot, int w);
// A weapon taken from the floor (`item` its row): chosen at once when the blaster is in hand in
// deathmatch, always outside it.
void fight_took(SequelServer& s, int slot, int item);
// The player's `use`, `weapnext` and `weapprev` commands; false when `cmd` is none of them.
bool fight_command(SequelServer& s, int slot, const std::string& cmd, const std::string& arg);

// Player `slot` dies, killed by `killer` (a slot; his own for a suicide, -1 the world) with weapon
// `weapon` (-1 none); the obituary is scored and printed.
void fight_kill(SequelServer& s, int slot, int killer, int weapon);

// The world's rows the fight writes: each live missile, and every player's held weapon numbers.
void fight_rows(SequelServer& s);
// The first world row of the missiles.
size_t fight_first_row(const SequelServer& s);

}  // namespace shooters_edge::server
