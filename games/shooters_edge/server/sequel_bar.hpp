// THE SEQUEL'S STATUS BAR, SERVER SIDE: what a sequel client's HUD reads off the server. The
// server sends a little program in the layout language (hud/sequel_layout.hpp) once a level, and
// every frame each player's 32 stats, which the program reads: numbers, and pictures and names by
// their index into the image and item strings it also sends. This file writes all three into the
// world's rows (rules_world.hpp: `status_bar`, `images`, `item_names`, each player's `stats`), so
// the window's own HUD, a recording, TV and a stock client draw the same bar.
//
// The stats (porting notes: sequel-hud.md § 4, § 8): 0 the health icon, 1 health, 2 and 3 the
// ammo of the weapon in hand (none for the blaster), 4 and 5 the armour worn (power armour
// switched on shows its cells instead, in turn every 0.8 s when both are worn), 6 and 12 the
// selected item, 7 and 8 the item last picked up (for 3 s), 9 and 10 the first power-up running
// (quad, invulnerability, suit, rebreather) and its whole seconds left, 11 the help computer's icon
// flashing while its news is unread, 14 the frags, 15 the blow taken this frame (bit 1 health,
// bit 2 armour). 13 (the scoreboard and help screens), 16 and 17 (spectators) stay 0 for now.
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
// ge:summary The sequel's status bar from the server: its program, stats, pictures and item names
// ge:keywords sequel status-bar stats layout pickup-icon timer help-icon selected-item item-number
// ge:keywords image-index configstring statusbar sequel-hud flash field_3
// ge:skill shooter ui
// ge:test tests/shooters_edge_sequel_bar_test.cpp
#pragma once

#include <string_view>

namespace shooters_edge::server {

struct SequelServer;

inline constexpr int kSqItemNumbers = 42;  // the game's item list: 0 none, 1..41
inline constexpr int kSqBlasterNumber = 7;

// Item row `row` of sequel_items.hpp's table as the game's item list numbers it (every health row
// is 41); 0 for none.
int sq_item_number(int row);
// The number of a weapon (sequel_combat.hpp's `SqWeapon`): the blaster 7, the hand grenades 12.
int sq_weapon_number(int weapon);
// Whether picking up item number `n` makes it the selected item (it has a use).
bool sq_selects(int n);
// An item number's icon (its picture's name under `pics/`), "" for none.
std::string_view sq_icon(int n);

// The program, the pictures and the item names into the world's rows (once the map is loaded).
void bar_start(SequelServer& s);
// Each player's stats into the world's rows (with the rest of the rows, each frame).
void bar_rows(SequelServer& s);

}  // namespace shooters_edge::server
