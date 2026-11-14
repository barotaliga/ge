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
    // The older ammunition box: bit 2 rockets, bit 4 nails, else shells (bit 8 the large one).
    {"classic", "sequel", "item_weapon", "ammo_rockets", 2, 0},
    {"classic", "sequel", "item_weapon", "ammo_bullets", 4, 0},
    {"classic", "sequel", "item_weapon", "ammo_shells", 0, 0},
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

// THE BALANCE the role table alone would miss: a map is a balance of strong and weak things, so
// after each item takes its match by role, the map as a whole is weighed in the target game's set.
//
// - **Weapons by tier** (`kWeaponTiers`): each game's weapons ranked 1 (a starting weapon's kin)
//   to 4; tier 3 and up is STRONG. A remixed map keeps between a floor (`RemixBalance::strong_min`,
//   when it has two or more weapons) and a share (`strong_share` of its weapons, rounded up) of
//   strong ones: short of the floor, its best weak weapon is raised to the strong weapon the map
//   has fewest of; past the share, strong ones are lowered to the target's best weak one the map
//   has fewest of. No more than `strong_same` of one strong weapon are kept: the rest are lowered
//   too. Which copy is lowered: the one nearest a strong pickup (`kStrongPickups`: the best armours
//   and the mega health), so the power is spread across the map; with none on the map, the last in
//   it.
// - **Pools** (`kPoolItems`, `kPoolOutputs`): where one game scatters small pickups the other
//   does not have (the sequel's stimpacks and health boxes, its armour shards), the ones lying
//   within `pool_radius` of each other are added up and laid down again as the target's own sizes,
//   the largest that fits first, on the spots they lay on: four stimpacks and a box of ten make
//   one fifteen, three boxes of twenty-five make two twenty-fives and a fifteen. A pool too small
//   for the target's smallest by half is left out.
struct WeaponTier {
  std::string_view game;
  std::string_view class_name;
  int32_t tier = 1;
};

inline constexpr WeaponTier kWeaponTiers[] = {
    {"classic", "weapon_supershotgun", 1},
    {"classic", "weapon_nailgun", 1},
    {"classic", "weapon_supernailgun", 2},
    {"classic", "weapon_grenadelauncher", 2},
    {"classic", "weapon_rocketlauncher", 3},
    {"classic", "weapon_lightning", 3},
    {"sequel", "weapon_shotgun", 1},
    {"sequel", "weapon_supershotgun", 1},
    {"sequel", "weapon_machinegun", 1},
    {"sequel", "weapon_chaingun", 2},
    {"sequel", "weapon_grenadelauncher", 2},
    {"sequel", "weapon_hyperblaster", 2},
    {"sequel", "weapon_rocketlauncher", 3},
    {"sequel", "weapon_railgun", 3},
    {"sequel", "weapon_bfg", 4},
};
inline constexpr int32_t kStrongTier = 3;

// A pickup that makes the place it lies strong, in the target's names and variant bits (-1: any):
// a strong weapon beside one is the first lowered.
struct StrongPickup {
  std::string_view game;
  std::string_view class_name;
  int32_t bits = -1;
};

inline constexpr StrongPickup kStrongPickups[] = {
    {"classic", "item_armor2", -1},      {"classic", "item_armorInv", -1},
    {"classic", "item_health", 2},  // the mega health
    {"sequel", "item_armor_combat", -1}, {"sequel", "item_armor_body", -1},
    {"sequel", "item_power_shield", -1}, {"sequel", "item_health_mega", -1},
};

struct RemixBalance {
  int32_t strong_min = 1;     // strong weapons at least, on a map of two weapons or more
  float strong_share = 0.4f;  // strong weapons at most: this share of them, rounded up
  int32_t strong_same = 2;    // copies of one strong weapon at most (0: no limit)
  float pool_radius = 160;    // u: small pickups this near one another are added up
};

// A small pickup one game scatters, by the pool it adds to and what it is worth there.
struct PoolItem {
  std::string_view from, to;  // the direction it pools in
  std::string_view class_name;
  std::string_view pool;  // "health", "armour"
  int32_t amount = 0;
};
// What a pool is laid down as in the target game: the largest first.
struct PoolOutput {
  std::string_view to;
  std::string_view pool;
  std::string_view class_name;
  int32_t bits = 0;
  int32_t amount = 0;
};

inline constexpr PoolItem kPoolItems[] = {
    {"sequel", "classic", "item_health_small", "health", 2},
    {"sequel", "classic", "item_health", "health", 10},
    {"sequel", "classic", "item_health_large", "health", 25},
    {"sequel", "classic", "item_armor_shard", "armour", 2},
};
// The classic game's health comes as fifteen (bit 1) and twenty-five; twenty shard points make a
// green armour.
inline constexpr PoolOutput kPoolOutputs[] = {
    {"classic", "health", "item_health", 0, 25},
    {"classic", "health", "item_health", 1, 15},
    {"classic", "armour", "item_armor1", 0, 20},
};

// The tier of `class_name` in `game` (0: not a ranked weapon).
inline int32_t weapon_tier(std::string_view game, std::string_view class_name) {
  for (const WeaponTier& w : kWeaponTiers)
    if (w.game == game && w.class_name == class_name) return w.tier;
  return 0;
}

// The pool row of `class_name` from `from` to `to`, or nullptr.
inline const PoolItem* pool_item(std::string_view from, std::string_view to,
                                 std::string_view class_name) {
  for (const PoolItem& p : kPoolItems)
    if (p.from == from && p.to == to && p.class_name == class_name) return &p;
  return nullptr;
}

// WHERE AN ITEM STANDS: the games measure an item's place from different points of its box. The
// classic game's ammunition and health boxes stand on their origin as a corner (their box runs 0
// to 32 across), its weapons and armour on their origin as the middle of their floor, its
// artifacts 24 above their floor; the sequel's items are boxes 15 each way about their origin. A
// rewritten item keeps the middle of its floor where it was (`foot`: from the origin to that
// point), so an item dropped onto its floor does not start inside it. `kFootLift` raises the
// new place off the floor: a box resting on a plane counts as in it.
struct ItemFoot {
  std::string_view game;
  std::string_view class_name;  // a class, or a prefix ending in '_'; "" the game's other items
  float foot[3] = {0, 0, 0};
};

inline constexpr ItemFoot kItemFeet[] = {
    {"classic", "item_health", {16, 16, 0}},
    {"classic", "item_shells", {16, 16, 0}},
    {"classic", "item_spikes", {16, 16, 0}},
    {"classic", "item_rockets", {16, 16, 0}},
    {"classic", "item_cells", {16, 16, 0}},
    {"classic", "item_weapon", {16, 16, 0}},
    {"classic", "item_artifact_", {0, 0, -24}},
    {"classic", "", {0, 0, 0}},
    {"sequel", "", {0, 0, -15}},
};
inline constexpr float kFootLift = 1;

// The foot of `class_name` in `game` (above): its own row, else its prefix's, else the game's.
inline const float* item_foot(std::string_view game, std::string_view class_name) {
  const float* fallback = nullptr;
  const float* by_prefix = nullptr;
  for (const ItemFoot& f : kItemFeet) {
    if (f.game != game) continue;
    if (f.class_name == class_name) return f.foot;
    if (f.class_name.empty())
      fallback = f.foot;
    else if (f.class_name.back() == '_' &&
             class_name.substr(0, f.class_name.size()) == f.class_name)
      by_prefix = f.foot;
  }
  return by_prefix != nullptr ? by_prefix : fallback;
}

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
