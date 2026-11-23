// QUAKE II'S STATUS BAR, SERVER SIDE: what a Quake II client's HUD reads off the server. The
// server sends a little program in the layout language (hud/q2_layout.hpp) once a level, and
// every frame each player's 32 stats, which the program reads: numbers, and pictures and names by
// their index into the image and item strings it also sends. This file writes all three into the
// world's rows (rules_world.hpp: `status_bar`, `images`, `item_names`, each player's `stats`), so
// the window's own HUD, a recording, TV and a stock client draw the same bar.
//
// The stats (porting notes: q2-hud.md § 4, § 8): 0 the health icon, 1 health, 2 and 3 the
// ammo of the weapon in hand (none for the blaster), 4 and 5 the armour worn (power armour
// switched on shows its cells instead, in turn every 0.8 s when both are worn), 6 and 12 the
// selected item, 7 and 8 the item last picked up (for 3 s), 9 and 10 the first power-up running
// (quad, invulnerability, suit, rebreather) and its whole seconds left, 11 the help computer's icon
// flashing while its news is unread, 14 the frags, 15 the blow taken this frame (bit 1 health,
// bit 2 armour). 13 bit 1 a screen up over the bar, bit 2 the inventory (q2_screens.hpp), 16
// and 17 (spectators) stay 0 for now.
//
// The items are numbered as the game's own list numbers them (1 the body armour .. 41 health; 0
// none), so an item's name is item string 1056 + its number, and a stock client's `use` and
// inventory line up with ours. Pictures are registered in a fixed order at the map's start.
//
// The two programs are our own, written from the bar's element positions: single player and
// co-op (health, ammo, armour, the selected item on the bottom row; the pickup, the timer and the
// help icon above it) and deathmatch (the same, the timer 16 px further left, the frags in the top
// right corner, and the spectator's lines).
//
// ge:summary Quake II's status bar from the server: its program, stats, pictures and item names
// ge:keywords q2 status-bar stats layout pickup-icon timer help-icon selected-item item-number
// ge:keywords image-index configstring statusbar q2-hud flash field_3
// ge:skill shooter ui
// ge:test tests/shooters_edge_q2_bar_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "shooters_edge/server/q2_pack.hpp"

namespace shooters_edge::server {

struct Q2Server;
struct SqBody;

// The base game's item list: 0 none, 1..41 (q2_pack.hpp: a pack's list is `sq_numbers(pack)`).
inline constexpr int kSqItemNumbers = 42;
inline constexpr int kSqBlasterNumber = 7;

// Every function below numbers items as `pack`'s own list does (q2_pack.hpp), the base game's by
// default; a server passes its own pack (`Q2ServerOptions::pack`).

// The size of `pack`'s item list (0 none, 1.. its items).
int sq_numbers(Q2Pack pack = Q2Pack::base);
// The number of the item called `classname` in `pack`'s list (every health is `item_health`, the
// blaster `weapon_blaster`); 0 when it is not in it.
int sq_number_of(std::string_view classname, Q2Pack pack = Q2Pack::base);
// Item row `row` of q2_items.hpp's table as the game's item list numbers it (every health row
// is one number); 0 for none.
int sq_item_number(int row, Q2Pack pack = Q2Pack::base);
// The number of a weapon (q2_combat.hpp's `SqWeapon`): the blaster 7, the hand grenades 12.
int sq_weapon_number(int weapon, Q2Pack pack = Q2Pack::base);
// Whether item number `n` is a weapon (the blaster and the hand grenades too), or a power-up
// with a use.
bool sq_weapon_numbered(int n, Q2Pack pack = Q2Pack::base);
bool sq_powerup_numbered(int n, Q2Pack pack = Q2Pack::base);
// Whether picking up item number `n` makes it the selected item (it has a use).
bool sq_selects(int n, Q2Pack pack = Q2Pack::base);
// How many of item number `n` the body holds, as the inventory counts it: a weapon or key 1 each,
// ammo its rounds, the armour worn its points; 0 for the shard and health.
int sq_count(const SqBody& body, int n, Q2Pack pack = Q2Pack::base);
// Item number `n`'s name as a player reads it ("Shotgun"), "" for none.
std::string_view sq_item_name(int n, Q2Pack pack = Q2Pack::base);
// An item number's icon (its picture's name under `pics/`), "" for none.
std::string_view sq_icon(int n, Q2Pack pack = Q2Pack::base);

// The bar's program: single player and co-op's, or deathmatch's (`match`: the frags too).
std::string sq_bar_program(bool match);

// The program, the pictures and the item names into the world's rows (once the map is loaded).
void bar_start(Q2Server& s);
// Each player's stats into the world's rows (with the rest of the rows, each frame).
void bar_rows(Q2Server& s);

}  // namespace shooters_edge::server
