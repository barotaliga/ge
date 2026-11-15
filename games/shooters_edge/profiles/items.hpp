// A CLASSIC GAME'S ITEMS AND SOUNDS AS DATA: what the bots, the HUD and the team's words know of a
// game's pickups, one table a game named by its profile row (game_profile.hpp).
//
// - **Item rows** (`ItemRow`): a map entity's class name (a trailing `*` matches any rest of
//   it), the spawnflags bits it needs (all of them; the rows are tried in order, so a variant's
//   row comes before the plain one), and what it sets: the kind, the amount (doubled by
//   spawnflags 1 when `doubles`), the respawn time, a weapon's bit and the name (empty: the
//   class name). A field at -1 (or `sets_kind` false) leaves the bot's default.
// - **Words** (`ItemWords`): an item's call word (what a teammate says: "ra", "rl") and the word
//   its pickup sound is heard as, by kind, and for a weapon by its bit (0 matches any).
// - **Names** (`ItemName`): what a pickup calls an item in the game's own words, by its class
//   name ("You got the Nailgun"). The window puts them in the server's local info under each
//   class name (`item_name_lines`), where the baseline mod's `name_of` reads them; an item the
//   table leaves out keeps the mod's own word, and a game with no table speaks the mod's words.
//   An ammunition's name is also its unit in a backpack's list ("20 shells").
// - **Sounds**: the samples a listener reads (plugins/shooter/hearing.hpp's `SoundMeaning`):
//   pickups, powers, spawns, fire, pain and death, with each pickup's heard word and each
//   weapon's fire as its call word (what a listener tells the enemy's weapon by).
//
// ge:summary A classic game's items, their call words and the sounds a bot reads, as data rows
// ge:keywords items item-row call-word heard-word pickup sound-table item-kind spawnflags
// ge:keywords respawn classname classic-items kclassicitems kclassicsounds
// ge:keywords item-name pickup-message pickup-print item_name_lines kclassicnames localinfo
// ge:keywords monster-models kclassicmonsters
// ge:skill shooter bots
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooter/hearing.hpp"
#include "shooter/item_tables.hpp"

namespace shooters_edge::profiles {

using ge::shooter::item_name_lines;
using ge::shooter::item_row;
using ge::shooter::item_words;
using ge::shooter::ItemKind;
using ge::shooter::ItemName;
using ge::shooter::ItemRow;
using ge::shooter::ItemTables;
using ge::shooter::ItemWords;
using ge::shooter::kItemKinds;

inline constexpr ItemRow kClassicItems[] = {
    {.class_name = "item_shells", .kind = ItemKind::shells, .amount = 20, .doubles = true},
    {.class_name = "item_spikes", .kind = ItemKind::nails, .amount = 25, .doubles = true},
    {.class_name = "item_rockets", .kind = ItemKind::rockets, .amount = 5, .doubles = true},
    {.class_name = "item_cells", .kind = ItemKind::cells, .amount = 6, .doubles = true},
    {.class_name = "item_artifact_invulnerability",
     .kind = ItemKind::pent,
     .respawn_s = 60,
     .name = "pent"},
    {.class_name = "item_artifact_super_damage",
     .kind = ItemKind::quad,
     .respawn_s = 60,
     .name = "quad"},
    {.class_name = "item_health",
     .needs_flags = 2,
     .kind = ItemKind::mega,
     .amount = 100,
     .name = "mega health"},
    {.class_name = "item_health",
     .needs_flags = 1,
     .kind = ItemKind::health,
     .amount = 15,
     .name = "health"},
    {.class_name = "item_health", .kind = ItemKind::health, .amount = 25, .name = "health"},
    {.class_name = "item_armor1", .kind = ItemKind::armor1},
    {.class_name = "item_armor2", .kind = ItemKind::armor2},
    {.class_name = "item_armorInv", .kind = ItemKind::armor3},
    {.class_name = "item_armor*", .sets_kind = false},
    {.class_name = "weapon_supershotgun", .kind = ItemKind::weapon, .respawn_s = 30, .weapon = 2},
    {.class_name = "weapon_nailgun", .kind = ItemKind::weapon, .respawn_s = 30, .weapon = 4},
    {.class_name = "weapon_supernailgun", .kind = ItemKind::weapon, .respawn_s = 30, .weapon = 8},
    {.class_name = "weapon_grenadelauncher",
     .kind = ItemKind::weapon,
     .respawn_s = 30,
     .weapon = 16},
    {.class_name = "weapon_rocketlauncher",
     .kind = ItemKind::weapon,
     .respawn_s = 30,
     .weapon = 32},
    {.class_name = "weapon_lightning", .kind = ItemKind::weapon, .respawn_s = 30, .weapon = 64},
};

inline constexpr ItemWords kClassicWords[] = {
    {ItemKind::health, 0, "health", "health"}, {ItemKind::mega, 0, "mh", "mh"},
    {ItemKind::armor1, 0, "ga", "armor"},      {ItemKind::armor2, 0, "ya", "armor"},
    {ItemKind::armor3, 0, "ra", "armor"},      {ItemKind::shells, 0, "shells", "ammo"},
    {ItemKind::nails, 0, "nails", "ammo"},     {ItemKind::rockets, 0, "rox", "ammo"},
    {ItemKind::cells, 0, "cells", "ammo"},     {ItemKind::quad, 0, "quad", "quad"},
    {ItemKind::pent, 0, "pent", "pent"},       {ItemKind::weapon, 2, "ssg", "weapon"},
    {ItemKind::weapon, 4, "ng", "weapon"},     {ItemKind::weapon, 8, "sng", "weapon"},
    {ItemKind::weapon, 16, "gl", "weapon"},    {ItemKind::weapon, 32, "rl", "weapon"},
    {ItemKind::weapon, 0, "lg", "weapon"},  // any other weapon
};

// The classic game's pickups: ammunition by its kind ("You got the rockets", a backpack's "5
// rockets"), weapons and powers by their own names. Its armour is "armor" whatever its colour,
// which is the mod's own line, so the armour rows are left out.
inline constexpr ItemName kClassicNames[] = {
    {"item_shells", "shells"},
    {"item_spikes", "nails"},
    {"item_rockets", "rockets"},
    {"item_cells", "cells"},
    {"item_armor1", "Green Armor"},
    {"item_armor2", "Yellow Armor"},
    {"item_armorInv", "Red Armor"},
    {"weapon_supershotgun", "Double-barrelled Shotgun"},
    {"weapon_nailgun", "nailgun"},
    {"weapon_supernailgun", "Super Nailgun"},
    {"weapon_grenadelauncher", "Grenade Launcher"},
    {"weapon_rocketlauncher", "Rocket Launcher"},
    {"weapon_lightning", "Thunderbolt"},
    {"item_artifact_super_damage", "Quad Damage"},
    {"item_artifact_invulnerability", "Pentagram of Protection"},
    {"item_artifact_invisibility", "Ring of Shadows"},
    {"item_artifact_envirosuit", "Biosuit"},
};

using ge::shooter::SoundClass;
inline constexpr ge::shooter::SoundMeaning kClassicSounds[] = {
    {"items/armor1.wav", SoundClass::pickup, true, "armor"},
    {"items/r_item2.wav", SoundClass::pickup, true, "mh"},
    {"items/r_item1.wav", SoundClass::pickup, false, "health"},
    {"items/health1.wav", SoundClass::pickup, false, "health"},
    {"weapons/pkup.wav", SoundClass::pickup, true, "weapon"},
    {"weapons/lock4.wav", SoundClass::pickup, false, "ammo"},
    {"items/damage.wav", SoundClass::power, true, "quad"},
    {"items/protect.wav", SoundClass::power, true, "pent"},
    {"misc/r_tele1.wav", SoundClass::spawn, true, "spawn"},
    {"weapons/ax1.wav", SoundClass::fire, false, "axe"},
    {"weapons/guncock.wav", SoundClass::fire, false, "sg"},
    {"weapons/shotgn2.wav", SoundClass::fire, false, "ssg"},
    {"weapons/rocket1i.wav", SoundClass::fire, false, "ng"},
    {"weapons/spike2.wav", SoundClass::fire, false, "sng"},
    {"weapons/grenade.wav", SoundClass::fire, false, "gl"},
    {"weapons/sgun1.wav", SoundClass::fire, false, "rl"},
    {"weapons/lhit.wav", SoundClass::fire, false, "lg"},
    {"player/pain1.wav", SoundClass::pain, false, ""},
    {"player/drown1.wav", SoundClass::pain, false, ""},
    {"player/death1.wav", SoundClass::death, false, ""},
    {"player/udeath.wav", SoundClass::death, false, ""},
};

// The monsters' models a co-op companion knows on sight (bots/companion.hpp): a client is sent
// each entity's model, and these are the ones that walk, fly or swim at the players.
inline constexpr std::string_view kClassicMonsters[] = {
    "progs/soldier.mdl", "progs/dog.mdl",      "progs/knight.mdl",   "progs/hknight.mdl",
    "progs/ogre.mdl",    "progs/zombie.mdl",   "progs/wizard.mdl",   "progs/fish.mdl",
    "progs/demon.mdl",   "progs/shambler.mdl", "progs/shalrath.mdl", "progs/enforcer.mdl",
    "progs/tarbaby.mdl", "progs/boss.mdl",     "progs/oldone.mdl"};
// Of those, the ones only a blast keeps down (a shot one gets up again), and the ones no weapon
// kills at all (the map's own trap does): a player passes them rather than fights them.
inline constexpr std::string_view kClassicBlastOnly[] = {"progs/zombie.mdl"};
inline constexpr std::string_view kClassicNoWeapon[] = {"progs/boss.mdl", "progs/oldone.mdl"};

inline constexpr ItemTables kClassicTables{kClassicItems, kClassicWords, kClassicSounds,
                                           kClassicNames};

}  // namespace shooters_edge::profiles
