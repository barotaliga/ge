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
// - **Sounds**: the samples a listener reads (plugins/shooter/hearing.hpp's `SoundMeaning`):
//   pickups, powers, spawns, fire, pain and death, with each pickup's heard word and each
//   weapon's fire as its call word (what a listener tells the enemy's weapon by).
//
// ge:summary A classic game's items, their call words and the sounds a bot reads, as data rows
// ge:keywords items item-row call-word heard-word pickup sound-table item-kind spawnflags
// ge:keywords respawn classname classic-items kclassicitems kclassicsounds
// ge:skill shooter bots
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "shooter/hearing.hpp"

namespace shooters_edge::profiles {

enum class ItemKind : uint8_t {
  health,
  mega,
  armor1,
  armor2,
  armor3,
  shells,
  nails,
  rockets,
  cells,
  weapon,
  quad,
  pent,  // the shield (no damage taken)
};

struct ItemRow {
  std::string_view class_name{};  // a trailing '*' matches any rest
  int32_t needs_flags = 0;        // spawnflags bits all present
  bool sets_kind = true;
  ItemKind kind = ItemKind::health;
  float amount = -1;        // health given or ammunition; -1: unset
  bool doubles = false;     // spawnflags 1 doubles the amount
  float respawn_s = -1;     // -1: the default
  int32_t weapon = 0;       // a weapon's bit (0: none)
  std::string_view name{};  // empty: the class name
};

struct ItemWords {
  ItemKind kind;
  int32_t weapon;          // for a weapon: its bit, 0 any
  std::string_view call;   // said of it
  std::string_view heard;  // its pickup sound's word
};

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

// A game's three tables, which its profile row carries (game_profile.hpp).
struct ItemTables {
  std::span<const ItemRow> items;                     // the map's items by class name
  std::span<const ItemWords> words;                   // their call and heard words
  std::span<const ge::shooter::SoundMeaning> sounds;  // the samples a listener reads
};
inline constexpr ItemTables kClassicTables{kClassicItems, kClassicWords, kClassicSounds};

// The row `class_name` with `spawnflags` matches first, or nullptr.
inline const ItemRow* item_row(std::span<const ItemRow> rows, std::string_view class_name,
                               int32_t spawnflags) {
  for (const ItemRow& r : rows) {
    const bool prefix = !r.class_name.empty() && r.class_name.back() == '*';
    const std::string_view stem =
        prefix ? r.class_name.substr(0, r.class_name.size() - 1) : r.class_name;
    const bool named = prefix ? class_name.substr(0, stem.size()) == stem : class_name == stem;
    if (named && (spawnflags & r.needs_flags) == r.needs_flags) return &r;
  }
  return nullptr;
}

// The words of a `kind` (and a weapon's `weapon` bit), or nullptr.
inline const ItemWords* item_words(std::span<const ItemWords> words, ItemKind kind,
                                   int32_t weapon) {
  for (const ItemWords& w : words) {
    if (w.kind == kind && (w.weapon == 0 || w.weapon == weapon)) return &w;
  }
  return nullptr;
}

}  // namespace shooters_edge::profiles
