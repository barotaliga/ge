// CLASSIC REMIX'S ITEM TABLE: what each classic game's map items become in another game's set
// (docs/goals/M18u.md). Each game lists its items by the shared roles of
// `plugins/classic/item_roles.hpp`: how it reads a map's item as a role, and how it lays a role
// down. A swap reads the item in one game's list and lays its role down from the other's, so a game
// added later lists its own items once and swaps with every game here. Two remixes use it:
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
//   weapon spends, health and armour by size, a powerup by what it does. A role the other game
//   has no spawn for drops the item: nothing in its set plays the part (an armour shard, a
//   silencer).
// - **Variant bits** (`kItemBits`, the low eight spawnflags bits) say which variant an item is
//   (the classic game's small and mega health, its large ammunition boxes); a reading needs them
//   all, the readings are tried in order (a variant's before the plain one), and the spawn's bits
//   replace them. The bits above (which skills and modes the item is left out of) mean the same in
//   both games and are kept.
// - **Not an item** (no prefix of `kItemPrefixes`): kept as it is (spawn points, lights, doors).
//   An item its game does not read is dropped and counted, so a map never spawns a class the rules
//   do not know.
//
// ge:summary Classic remix's item table: each game's map items as another game's, by role
// ge:keywords remix classic-remix item-table item-swap class-name spawnflags cr_items
// ge:keywords remix_item item-roles item-bits dropped by-role map-items other-game game_items
// ge:skill shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "classic/item_roles.hpp"

namespace shooters_edge::profiles {

// The spawnflags bits that say an item's variant; the rest are kept through a remix.
inline constexpr int32_t kItemBits = ge::classic::kVariantBits;

// The class names that are items, in either game: an item its game does not read is dropped.
inline constexpr std::string_view kItemPrefixes[] = {"weapon_", "item_", "ammo_", "key_"};

using ge::classic::GameItems;
using ge::classic::ItemReading;
using ge::classic::ItemRole;
using ge::classic::ItemSpawn;
using ge::classic::ItemSwap;

// The classic game's items. Its small health box is the rotten one (bit 1), its mega health bit 2,
// its large ammunition boxes bit 1. It lays every shotgun down as its double-barrelled one (the
// single one is the weapon a player starts with) and every energy weapon as its beam.
inline constexpr ItemReading kClassicReads[] = {
    {"weapon_supershotgun", 0, ItemRole::scatter_heavy},
    {"weapon_nailgun", 0, ItemRole::rapid},
    {"weapon_supernailgun", 0, ItemRole::rapid_heavy},
    {"weapon_grenadelauncher", 0, ItemRole::lobber},
    {"weapon_rocketlauncher", 0, ItemRole::launcher},
    {"weapon_lightning", 0, ItemRole::beam},
    {"item_shells", 0, ItemRole::ammo_scatter},
    {"item_spikes", 0, ItemRole::ammo_rapid},
    {"item_rockets", 0, ItemRole::ammo_explosive},
    {"item_cells", 0, ItemRole::ammo_beam},
    // The older ammunition box: bit 2 rockets, bit 4 nails, else shells (bit 8 the large one).
    {"item_weapon", 2, ItemRole::ammo_explosive},
    {"item_weapon", 4, ItemRole::ammo_rapid},
    {"item_weapon", 0, ItemRole::ammo_scatter},
    {"item_health", 2, ItemRole::health_mega},
    {"item_health", 1, ItemRole::health_small},
    {"item_health", 0, ItemRole::health_large},
    {"item_armor1", 0, ItemRole::armour_light},
    {"item_armor2", 0, ItemRole::armour_medium},
    {"item_armorInv", 0, ItemRole::armour_heavy},
    {"item_artifact_super_damage", 0, ItemRole::damage},
    {"item_artifact_invulnerability", 0, ItemRole::invulnerable},
    {"item_artifact_envirosuit", 0, ItemRole::protect},
    {"item_artifact_invisibility", 0, ItemRole::stealth},
};
inline constexpr ItemSpawn kClassicSpawns[] = {
    {ItemRole::scatter, "weapon_supershotgun", 0},
    {ItemRole::scatter_heavy, "weapon_supershotgun", 0},
    {ItemRole::rapid, "weapon_nailgun", 0},
    {ItemRole::rapid_heavy, "weapon_supernailgun", 0},
    {ItemRole::lobber, "weapon_grenadelauncher", 0},
    {ItemRole::launcher, "weapon_rocketlauncher", 0},
    {ItemRole::beam, "weapon_lightning", 0},
    {ItemRole::energy, "weapon_lightning", 0},
    {ItemRole::blast, "weapon_lightning", 0},
    {ItemRole::ammo_scatter, "item_shells", 0},
    {ItemRole::ammo_scatter_large, "item_shells", 1},
    {ItemRole::ammo_rapid, "item_spikes", 0},
    {ItemRole::ammo_lobber, "item_rockets", 0},
    {ItemRole::ammo_explosive, "item_rockets", 0},
    {ItemRole::ammo_pack, "item_rockets", 1},
    {ItemRole::ammo_beam, "item_cells", 0},
    {ItemRole::ammo_energy, "item_cells", 0},
    {ItemRole::health_small, "item_health", 1},
    {ItemRole::health_large, "item_health", 0},
    {ItemRole::health_mega, "item_health", 2},
    {ItemRole::armour_light, "item_armor1", 0},
    {ItemRole::armour_medium, "item_armor2", 0},
    {ItemRole::armour_heavy, "item_armorInv", 0},
    {ItemRole::damage, "item_artifact_super_damage", 0},
    {ItemRole::invulnerable, "item_artifact_invulnerability", 0},
    {ItemRole::protect, "item_artifact_envirosuit", 0},
    {ItemRole::stealth, "item_artifact_invisibility", 0},
};

// The sequel's items. Its stimpack and box of ten both lay down as the other game's small health,
// its adrenaline and the ancient head as a mega health, its power armours by the armour they match.
inline constexpr ItemReading kSequelReads[] = {
    {"weapon_shotgun", 0, ItemRole::scatter},
    {"weapon_supershotgun", 0, ItemRole::scatter_heavy},
    {"weapon_machinegun", 0, ItemRole::rapid},
    {"weapon_chaingun", 0, ItemRole::rapid_heavy},
    {"weapon_grenadelauncher", 0, ItemRole::lobber},
    {"weapon_rocketlauncher", 0, ItemRole::launcher},
    {"weapon_hyperblaster", 0, ItemRole::energy},
    {"weapon_railgun", 0, ItemRole::beam},
    {"weapon_bfg", 0, ItemRole::blast},
    {"ammo_shells", 0, ItemRole::ammo_scatter},
    {"ammo_bullets", 0, ItemRole::ammo_rapid},
    {"ammo_grenades", 0, ItemRole::ammo_lobber},
    {"ammo_rockets", 0, ItemRole::ammo_explosive},
    {"ammo_cells", 0, ItemRole::ammo_energy},
    {"ammo_slugs", 0, ItemRole::ammo_beam},
    {"item_bandolier", 0, ItemRole::ammo_scatter_large},
    {"item_pack", 0, ItemRole::ammo_pack},
    {"item_health_small", 0, ItemRole::health_small},
    {"item_health", 0, ItemRole::health_small},
    {"item_health_large", 0, ItemRole::health_large},
    {"item_health_mega", 0, ItemRole::health_mega},
    {"item_adrenaline", 0, ItemRole::health_mega},
    {"item_ancient_head", 0, ItemRole::health_mega},
    {"item_armor_shard", 0, ItemRole::armour_shard},
    {"item_armor_jacket", 0, ItemRole::armour_light},
    {"item_armor_combat", 0, ItemRole::armour_medium},
    {"item_armor_body", 0, ItemRole::armour_heavy},
    {"item_power_screen", 0, ItemRole::armour_medium},
    {"item_power_shield", 0, ItemRole::armour_heavy},
    {"item_quad", 0, ItemRole::damage},
    {"item_invulnerability", 0, ItemRole::invulnerable},
    {"item_enviro", 0, ItemRole::protect},
    {"item_breather", 0, ItemRole::protect},
    {"item_silencer", 0, ItemRole::quiet},
};
// The classic game's stealth lays down as the sequel's adrenaline: the sequel has none.
inline constexpr ItemSpawn kSequelSpawns[] = {
    {ItemRole::scatter, "weapon_shotgun", 0},
    {ItemRole::scatter_heavy, "weapon_supershotgun", 0},
    {ItemRole::rapid, "weapon_machinegun", 0},
    {ItemRole::rapid_heavy, "weapon_chaingun", 0},
    {ItemRole::lobber, "weapon_grenadelauncher", 0},
    {ItemRole::launcher, "weapon_rocketlauncher", 0},
    {ItemRole::beam, "weapon_railgun", 0},
    {ItemRole::energy, "weapon_hyperblaster", 0},
    {ItemRole::blast, "weapon_bfg", 0},
    {ItemRole::ammo_scatter, "ammo_shells", 0},
    {ItemRole::ammo_scatter_large, "item_bandolier", 0},
    {ItemRole::ammo_rapid, "ammo_bullets", 0},
    {ItemRole::ammo_lobber, "ammo_grenades", 0},
    {ItemRole::ammo_explosive, "ammo_rockets", 0},
    {ItemRole::ammo_pack, "item_pack", 0},
    {ItemRole::ammo_beam, "ammo_slugs", 0},
    {ItemRole::ammo_energy, "ammo_cells", 0},
    {ItemRole::health_small, "item_health", 0},
    {ItemRole::health_large, "item_health_large", 0},
    {ItemRole::health_mega, "item_health_mega", 0},
    {ItemRole::armour_shard, "item_armor_shard", 0},
    {ItemRole::armour_light, "item_armor_jacket", 0},
    {ItemRole::armour_medium, "item_armor_combat", 0},
    {ItemRole::armour_heavy, "item_armor_body", 0},
    {ItemRole::damage, "item_quad", 0},
    {ItemRole::invulnerable, "item_invulnerability", 0},
    {ItemRole::protect, "item_enviro", 0},
    {ItemRole::stealth, "item_adrenaline", 0},
    {ItemRole::quiet, "item_silencer", 0},
};

// Every game's items, by its content profile id.
inline constexpr GameItems kGameItems[] = {
    {"classic", kClassicReads, kClassicSpawns},
    {"sequel", kSequelReads, kSequelSpawns},
};

// The items of `game`, or nullptr.
inline const GameItems* game_items(std::string_view game) {
  for (const GameItems& g : kGameItems)
    if (g.game == game) return &g;
  return nullptr;
}

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
// `ammo` is the box of what it fires, which a weak copy past `RemixBalance::weak_same` becomes.
struct WeaponTier {
  std::string_view game;
  std::string_view class_name;
  int32_t tier = 1;
  std::string_view ammo;
};

inline constexpr WeaponTier kWeaponTiers[] = {
    {"classic", "weapon_supershotgun", 1, "item_shells"},
    {"classic", "weapon_nailgun", 1, "item_spikes"},
    {"classic", "weapon_supernailgun", 2, "item_spikes"},
    {"classic", "weapon_grenadelauncher", 2, "item_rockets"},
    {"classic", "weapon_rocketlauncher", 3, "item_rockets"},
    {"classic", "weapon_lightning", 3, "item_cells"},
    {"sequel", "weapon_shotgun", 1, "ammo_shells"},
    {"sequel", "weapon_supershotgun", 1, "ammo_shells"},
    {"sequel", "weapon_machinegun", 1, "ammo_bullets"},
    {"sequel", "weapon_chaingun", 2, "ammo_bullets"},
    {"sequel", "weapon_grenadelauncher", 2, "ammo_grenades"},
    {"sequel", "weapon_hyperblaster", 2, "ammo_cells"},
    {"sequel", "weapon_rocketlauncher", 3, "ammo_rockets"},
    {"sequel", "weapon_railgun", 3, "ammo_slugs"},
    {"sequel", "weapon_bfg", 4, "ammo_cells"},
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
  int32_t weak_same = 2;      // copies of one weak weapon at most, the rest its ammo (0: no limit)
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

// `class_name` with `spawnflags` read in `from` and laid down in `to` (`ge::classic::swap_item`);
// not known when either game has no list or `from` does not read it (an item then is dropped).
inline ItemSwap remix_item(std::string_view from, std::string_view to, std::string_view class_name,
                           int32_t spawnflags) {
  const GameItems* f = game_items(from);
  const GameItems* t = game_items(to);
  if (f == nullptr || t == nullptr) return {};
  return ge::classic::swap_item(*f, *t, class_name, spawnflags);
}

}  // namespace shooters_edge::profiles
