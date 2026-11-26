// QUAKE'S WEAPONS IN QUAKE II'S RULES (classic remix's `cr_items q1`, the `sv_wear q1` the rules
// wear): Quake's eight fired whole, never Quake II's weapons dressed as Quake's. The rules stay
// Quake II's (the armour, the push, the items' timing); the weapons are Quake's: its rows
// (q2_combat.hpp's `sq_quake_row`, the numbers `profiles::kQ1Weapons` and classic/q1_arms.hpp
// hold), its shots, its missiles, its looks and its sounds.
//
// - **The kit**: a player spawns with the axe and the shotgun and 25 shells, the shotgun in hand,
//   and Quake's carry limits (100 shells, 200 nails, 100 rockets, 100 cells).
// - **The items** (`sq_quake_take`): each of Quake II's weapons and ammunition gives the Quake
//   weapon of its role and Quake's own amount: the shotguns the super shotgun with 5 shells, the
//   machinegun the nailgun and the chaingun the super nailgun with 30 nails, the launchers theirs
//   with 5 rockets, the hyperblaster, the railgun and the BFG the lightning gun with 15 cells;
//   shells 20, bullets 25 nails, grenades and rockets 5 rockets, cells and slugs 6 cells. A
//   weapon is kept as the item that stands for it in its row.
// - **The shots** (`quake_fire`): the axe's cut lands 0.2 s after the swing; the shotguns' pellets
//   from the eye; the nailgun's spikes from its two barrels in turn, the super nailgun's two
//   spikes' worth in one (one with a single nail left), the super shotgun a shotgun's with a
//   single shell; the grenade thrown up and forward; the rocket from 8 forward and 16 up; the
//   lightning gun's beam 600 long, under water a discharge of every cell.
// - **The missiles** (`quake_missile`): a spike harms what it meets and leaves its mark on a wall;
//   a grenade bounces off the walls, bursts on a body it touches or when its fuse ends; a rocket
//   bursts on what it meets, its hit 100 to 120, its blast 120.
// - **The view model's frame** (`quake_view_frame`): Quake's view models stand on frame 0 and
//   play their fire run from each shot (the held weapons' round while they fire).
//
// ge:summary Quake's eight weapons fired whole by Quake II's rules under classic remix
// ge:keywords q1-arms quake-weapons remix-weapons cr_items sv_wear real-weapons q2 quake_fire
// ge:keywords quake_missile sq_quake_take axe-cut nail-barrels lightning-beam discharge
// ge:skill shooter
// ge:test tests/shooters_edge_remix_q1_arms_test.cpp
#pragma once

#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_items.hpp"

namespace shooters_edge::server {

struct Q2Server;
struct Q2Player;
struct Q2Missile;

// What an item gives when Quake's weapons are worn: the item kept in its place (a weapon's row's
// item; -1 the item's own), the ammunition and how much. `ammo` none and `item` -1: the item's own.
struct SqQuakeTake {
  int item = -1;
  SqAmmo ammo = SqAmmo::none;
  int amount = 0;
};
SqQuakeTake sq_quake_take(int item);

// Quake's carry limits and starting kit into a new body (`quake` set).
void sq_quake_body(SqBody& body);

// The sounds Quake's weapons make beyond each row's own (the server registers them).
inline constexpr const char* kQuakeArmSounds[] = {"weapons/lstart.wav", "weapons/lhit.wav",
                                                  "weapons/r_exp3.wav", "weapons/bounce.wav",
                                                  "player/axhit2.wav"};

// Whether the server wears Quake's weapons, read from `sv_wear` as a map starts.
bool quake_arms_worn(const Q2Server& s);
// A spawn's hand: the shotgun, and the view model's runs cleared.
void quake_arms_spawn(Q2Server& s, int slot);
// The shot(s) of the weapon in `slot`'s hand this step (its ammunition already taken).
void quake_fire(Q2Server& s, int slot, int shots);
// The frame's own: the axe's cut landing, the held lightning gun let go.
void quake_arms_frame(Q2Server& s, int slot, bool attack);
// Moves one of Quake's missiles a frame; false for any other kind.
bool quake_missile(Q2Server& s, Q2Missile& m);
// The frame `p`'s view model shows.
int quake_view_frame(const Q2Server& s, const Q2Player& p);
// The words a kill by Quake's weapon `w` reads ("<victim> <words> <killer><tail>").
struct SqQuakeWords {
  const char* words = nullptr;
  const char* tail = "";
};
SqQuakeWords sq_quake_words(int w);

}  // namespace shooters_edge::server
