// The sequel runner's fighting half (sequel_server.hpp states the rules): the weapons' models and
// sounds listed at the map's start, the hand stepped and its shots made, missiles moved, damage
// dealt, deaths, gibs and obituaries, the weapon chosen, items dropped and taken back, and the
// world's rows for the shots in flight and the dropped items.
// Only sequel_server.cpp calls these.
//
// ge:summary The sequel runner's fight: shots, missiles, damage, deaths, the chosen weapon
// ge:keywords sequel fight shots hitscan missiles radius-damage obituary death frags use-weapon
// ge:keywords hand-grenade drop-item gibs weapons-stay quad-drop teams friendly-fire
// ge:keywords corpse-queue scores fov
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_fight_test.cpp
#pragma once

#include <string>
#include <vector>

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

// What killed a player when no weapon did (`fight_kill`'s `weapon`).
inline constexpr int kSqByFall = 100, kSqByLava = 101, kSqBySlime = 102, kSqByWater = 103;
// The hand grenade held to the end of its fuse.
inline constexpr int kSqByHeldGrenade = 104;
// A mover's crush, a hurting trigger, a teleport onto someone, a map's explosion.
inline constexpr int kSqByCrush = 105, kSqByHurt = 106, kSqByTelefrag = 107, kSqByExplosion = 108;

// A blow to player `target` from `attacker` (a slot or -1) along `dir` (zero: no push), by
// `weapon` (a weapon or a `kSqBy*`); `flags` 1 passes armour by, 2 passes invulnerability by.
void fight_hurt(SequelServer& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
                int weapon, int flags = 0);
// Radius damage at `at`: `damage` less half the distance, within `radius` (the attacker's half),
// to the players and the map's shootable movers it reaches.
void fight_blast(SequelServer& s, sh::Vec3f at, int attacker, int damage, float radius, int weapon);

// Player `slot` dies, killed by `killer` (a slot; his own for a suicide, -1 the world) with weapon
// `weapon` (-1 none, or a `kSqBy*`); the obituary is scored and printed.
void fight_kill(SequelServer& s, int slot, int killer, int weapon);

// Item `item` (a weapon, an ammo or a powerup, by `drop <its name>`) thrown from player `slot`;
// false, with a line to him, when it cannot be.
bool fight_drop(SequelServer& s, int slot, int item);

// Item `item` used from the inventory (a powerup or power armour; `use <its name>`); false when
// it is not one or none is held.
bool fight_use(SequelServer& s, int slot, int item);
// The end of player `slot`'s rules frame: the powerups' and the air's clocks, drowning, lava,
// slime and falling damage.
void fight_body_frame(SequelServer& s, int slot);
// The level's limits: true when the frag or time limit ended it (`next_map` named, the
// intermission due).
bool fight_limits(SequelServer& s);

// The items dropped (`drop`, or at death) falling to rest and vanishing when their time is up.
void fight_drops(SequelServer& s);
// The dropped items player `slot`'s box touches, taken by their rule.
void fight_touch_drops(SequelServer& s, int slot);
// Whether `dmflags` bit `bit` is set in deathmatch.
bool fight_dmflag(const SequelServer& s, int bit);

// The world's rows the fight writes: each live missile and dropped item, and every player's held
// weapon numbers.
void fight_rows(SequelServer& s);
// The first world row of the missiles; the dropped items' follow them.
size_t fight_first_row(const SequelServer& s);

// True when players `a` and `b` (two slots) are on one team: in deathmatch with `dmflags` 64 (by
// the skin of `skin`'s model/skin) or 128 (by the model), or in co-op (everyone).
bool fight_teammates(const SequelServer& s, int a, int b);
// Player `slot`'s field of view from his `fov` setting: 1..160, 90 when unset or with `dmflags`
// 32768 in deathmatch.
float fight_fov(const SequelServer& s, int slot);
// A dead player about to respawn leaves his body (not when it was thrown apart).
void fight_bury(SequelServer& s, int slot);
// The scores, best first: one line a player ("  12  Ana").
std::vector<std::string> fight_scores(const SequelServer& s);
// A line printed to player `slot`, or to everyone with -1.
void fight_print(SequelServer& s, int slot, const std::string& text);

}  // namespace shooters_edge::server
