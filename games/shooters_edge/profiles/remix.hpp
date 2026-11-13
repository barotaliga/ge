// CLASSIC REMIX'S ITEM TABLE: what each classic game's map items become in another game's set
// (docs/goals/M18u.md). One row a class name a direction: the game the map's item is from, the
// game whose set it becomes, the class name it has (with the spawnflags bits it needs, all of
// them), and the class name and item bits it takes. Two remixes read it:
//
// - **Another game's map** (`map <name>` finding the map in the other game's packs): its items
//   become the played game's own, so the map plays with the items the game's rules know.
// - **Another game's items on a game's own map** (`cr_items`): the map's items become the other
//   game's set, so the classic game's maps play with the sequel's weapons, and the other way.
//
// The rules of the table:
//
// - **By role, not by name**: a weapon takes the weapon of the same place in the other game's set
//   (the nail weapons the bullet ones, the beam the rail), an ammunition the ammunition its
//   weapon spends, health and armour by size, a powerup by what it does. Several rows may name
//   one target; a row whose target is empty drops the item (`kDropped`): nothing in the other
//   set plays its part (an armour shard, a silencer).
// - **Item bits** (`kItemBits`, the low eight spawnflags bits) say which variant an item is (the
//   classic game's small and mega health, its large ammunition boxes); a row matches them all,
//   the rows are tried in order (a variant's row before the plain one), and the target's bits
//   replace them. The bits above (which skills and modes the item is left out of) mean the same
//   in both games and are kept.
// - **Not an item** (no prefix of `kItemPrefixes`): kept as it is (spawn points, lights, doors).
//   An item with no row is dropped and counted, so a map never spawns a class the rules do not
//   know.
//
// ge:summary Classic remix's item table: each game's map items as another game's, by role
// ge:keywords remix classic-remix item-table item-swap class-name spawnflags cr_items
// ge:keywords remix_row item-bits dropped by-role map-items other-game
// ge:skill shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace shooters_edge::profiles {

// The spawnflags bits that say an item's variant; the rest are kept through a remix.
inline constexpr int32_t kItemBits = 0xff;

// The class names that are items, in either game: an item with no row is dropped.
inline constexpr std::string_view kItemPrefixes[] = {"weapon_", "item_", "ammo_", "key_"};

struct RemixRow {
  std::string_view from;        // the content profile id the map's item is from
  std::string_view to;          // the content profile id whose set it becomes
  std::string_view class_name;  // the map's class name
  std::string_view becomes;     // the class name it takes; empty: dropped
  int32_t needs = 0;            // item bits all present
  int32_t bits = 0;             // the item bits it takes
};

inline constexpr std::string_view kDropped{};

// The classic game's items as the sequel's. Its small health box is the rotten one (bit 1), its
// mega health bit 2, its large ammunition boxes bit 1.
inline constexpr RemixRow kRemixRows[] = {
    {"classic", "sequel", "weapon_supershotgun", "weapon_supershotgun", 0, 0},
    {"classic", "sequel", "weapon_nailgun", "weapon_machinegun", 0, 0},
    {"classic", "sequel", "weapon_supernailgun", "weapon_chaingun", 0, 0},
    {"classic", "sequel", "weapon_grenadelauncher", "weapon_grenadelauncher", 0, 0},
    {"classic", "sequel", "weapon_rocketlauncher", "weapon_rocketlauncher", 0, 0},
    {"classic", "sequel", "weapon_lightning", "weapon_railgun", 0, 0},
    {"classic", "sequel", "item_shells", "ammo_shells", 0, 0},
    {"classic", "sequel", "item_spikes", "ammo_bullets", 0, 0},
    {"classic", "sequel", "item_rockets", "ammo_rockets", 0, 0},
    {"classic", "sequel", "item_cells", "ammo_slugs", 0, 0},
    {"classic", "sequel", "item_health", "item_health_mega", 2, 0},
    {"classic", "sequel", "item_health", "item_health", 1, 0},
    {"classic", "sequel", "item_health", "item_health_large", 0, 0},
    {"classic", "sequel", "item_armor1", "item_armor_jacket", 0, 0},
    {"classic", "sequel", "item_armor2", "item_armor_combat", 0, 0},
    {"classic", "sequel", "item_armorInv", "item_armor_body", 0, 0},
    {"classic", "sequel", "item_artifact_super_damage", "item_quad", 0, 0},
    {"classic", "sequel", "item_artifact_invulnerability", "item_invulnerability", 0, 0},
    {"classic", "sequel", "item_artifact_envirosuit", "item_enviro", 0, 0},
    {"classic", "sequel", "item_artifact_invisibility", "item_adrenaline", 0, 0},

    // The sequel's items as the classic game's.
    {"sequel", "classic", "weapon_shotgun", "weapon_supershotgun", 0, 0},
    {"sequel", "classic", "weapon_supershotgun", "weapon_supershotgun", 0, 0},
    {"sequel", "classic", "weapon_machinegun", "weapon_nailgun", 0, 0},
    {"sequel", "classic", "weapon_chaingun", "weapon_supernailgun", 0, 0},
    {"sequel", "classic", "weapon_grenadelauncher", "weapon_grenadelauncher", 0, 0},
    {"sequel", "classic", "weapon_rocketlauncher", "weapon_rocketlauncher", 0, 0},
    {"sequel", "classic", "weapon_hyperblaster", "weapon_lightning", 0, 0},
    {"sequel", "classic", "weapon_railgun", "weapon_lightning", 0, 0},
    {"sequel", "classic", "weapon_bfg", "weapon_lightning", 0, 0},
    {"sequel", "classic", "ammo_shells", "item_shells", 0, 0},
    {"sequel", "classic", "ammo_bullets", "item_spikes", 0, 0},
    {"sequel", "classic", "ammo_grenades", "item_rockets", 0, 0},
    {"sequel", "classic", "ammo_rockets", "item_rockets", 0, 0},
    {"sequel", "classic", "ammo_cells", "item_cells", 0, 0},
    {"sequel", "classic", "ammo_slugs", "item_cells", 0, 0},
    {"sequel", "classic", "item_bandolier", "item_shells", 0, 1},
    {"sequel", "classic", "item_pack", "item_rockets", 0, 1},
    {"sequel", "classic", "item_health_small", "item_health", 0, 1},
    {"sequel", "classic", "item_health", "item_health", 0, 1},
    {"sequel", "classic", "item_health_large", "item_health", 0, 0},
    {"sequel", "classic", "item_health_mega", "item_health", 0, 2},
    {"sequel", "classic", "item_adrenaline", "item_health", 0, 2},
    {"sequel", "classic", "item_ancient_head", "item_health", 0, 2},
    {"sequel", "classic", "item_armor_shard", kDropped, 0, 0},
    {"sequel", "classic", "item_armor_jacket", "item_armor1", 0, 0},
    {"sequel", "classic", "item_armor_combat", "item_armor2", 0, 0},
    {"sequel", "classic", "item_armor_body", "item_armorInv", 0, 0},
    {"sequel", "classic", "item_power_screen", "item_armor2", 0, 0},
    {"sequel", "classic", "item_power_shield", "item_armorInv", 0, 0},
    {"sequel", "classic", "item_quad", "item_artifact_super_damage", 0, 0},
    {"sequel", "classic", "item_invulnerability", "item_artifact_invulnerability", 0, 0},
    {"sequel", "classic", "item_enviro", "item_artifact_envirosuit", 0, 0},
    {"sequel", "classic", "item_breather", "item_artifact_envirosuit", 0, 0},
    {"sequel", "classic", "item_silencer", kDropped, 0, 0},
};

// Whether `class_name` is an item in either game (`kItemPrefixes`).
inline bool is_item_class(std::string_view class_name) {
  for (std::string_view p : kItemPrefixes)
    if (class_name.substr(0, p.size()) == p) return true;
  return false;
}

// The first row from `from` to `to` for `class_name` whose item bits `spawnflags` holds; nullptr
// when none (an item then is dropped).
inline const RemixRow* remix_row(std::string_view from, std::string_view to,
                                 std::string_view class_name, int32_t spawnflags,
                                 std::span<const RemixRow> rows = kRemixRows) {
  for (const RemixRow& r : rows)
    if (r.from == from && r.to == to && r.class_name == class_name &&
        (spawnflags & r.needs & kItemBits) == r.needs)
      return &r;
  return nullptr;
}

}  // namespace shooters_edge::profiles
