// CLASSIC REMIX'S ITEM TABLE: what each classic game's map items become in another game's set
// (docs/goals/M18u.md). Each game lists its items by the shared roles of
// `plugins/classic/item_roles.hpp`: how it reads a map's item as a role, and how it lays a role
// down. A swap reads the item in one game's list and lays its role down from the other's, so a game
// added later lists its own items once and swaps with every game here. Two remixes use it:
//
// - **Another game's map** (`map <name>` finding the map in the other game's packs): its items
//   become the played game's own, so the map plays with the items the game's rules know.
// - **Another game's items on a game's own map** (`cr_items`): the map's items become the other
//   game's set, so Quake's maps play with Quake II's weapons, and the other way.
//
// The rules of the table:
//
// - **By role, not by name**: a weapon takes the weapon of the same place in the other game's set
//   (the nail weapons the bullet ones, the beam the rail), an ammunition the ammunition its
//   weapon spends, health and armour by size, a powerup by what it does. A role the other game
//   has no spawn for drops the item: nothing in its set plays the part (an armour shard, a
//   silencer).
// - **Variant bits** (`kItemBits`, the low eight spawnflags bits) say which variant an item is
//   (Quake's small and mega health, its large ammunition boxes); a reading needs them
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
// ge:keywords mission-pack lava-spikes multi-rockets plasma
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

// Quake's items. Its small health box is the rotten one (bit 1), its mega health bit 2,
// its large ammunition boxes bit 1. It lays every shotgun down as its double-barrelled one (the
// single one is the weapon a player starts with) and every energy weapon as its beam.
inline constexpr ItemReading kQ1Reads[] = {
    {"weapon_supershotgun", 0, ItemRole::scatter_heavy},
    {"weapon_nailgun", 0, ItemRole::rapid},
    {"weapon_supernailgun", 0, ItemRole::rapid_heavy},
    {"weapon_grenadelauncher", 0, ItemRole::lobber},
    {"weapon_rocketlauncher", 0, ItemRole::launcher},
    {"weapon_lightning", 0, ItemRole::beam},
    // Scourge of Armagon's weapons: the laser cannon's bouncing bolts are the rapid energy weapon,
    // Mjolnir's lightning across the floor the room-clearing one, the proximity gun's waiting
    // grenades the lobber. Its empathy shields and horn of conjuring have no part in another set
    // and drop; its wetsuit is the suit against the water.
    {"weapon_laser_gun", 0, ItemRole::energy},
    {"weapon_mjolnir", 0, ItemRole::blast},
    {"weapon_proximity_gun", 0, ItemRole::lobber},
    {"item_artifact_wetsuit", 0, ItemRole::protect},
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
    // Dissolution of Eternity's ammunition for its weapons' second kinds (its program turns the
    // nailguns, the launchers and the lightning gun over to them): the ammunition of the same
    // weapon. Its power shield, anti-grav belt, vengeance sphere and random powerup play no part
    // another game has, so they drop.
    {"item_lava_spikes", 0, ItemRole::ammo_rapid},
    {"item_multi_rockets", 0, ItemRole::ammo_explosive},
    {"item_plasma", 0, ItemRole::ammo_energy},
};
inline constexpr ItemSpawn kQ1Spawns[] = {
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

// Quake II's items. Its stimpack and box of ten both lay down as the other game's small health,
// its adrenaline and the ancient head as a mega health, its power armours by the armour they match.
inline constexpr ItemReading kQ2Reads[] = {
    {"weapon_shotgun", 0, ItemRole::scatter},
    {"weapon_supershotgun", 0, ItemRole::scatter_heavy},
    {"weapon_machinegun", 0, ItemRole::rapid},
    {"weapon_chaingun", 0, ItemRole::rapid_heavy},
    {"weapon_grenadelauncher", 0, ItemRole::lobber},
    {"weapon_rocketlauncher", 0, ItemRole::launcher},
    {"weapon_hyperblaster", 0, ItemRole::energy},
    {"weapon_railgun", 0, ItemRole::beam},
    {"weapon_bfg", 0, ItemRole::blast},
    // Ground Zero's weapons: the ETF rifle's flechettes are the light rapid-fire weapon, the plasma
    // beam (by either of its names) the beam, the prox launcher's waiting grenades the lobber, the
    // disruptor's slow tracking bolt the heavy single shot a rocket launcher is. The chainfist
    // (a fist), the tesla and the A-M bomb (thrown from the inventory) have no part in another set
    // and drop, as do the spheres, the decoy and the goggles; double damage is the damage powerup.
    {"weapon_etf_rifle", 0, ItemRole::rapid},
    {"weapon_heatbeam", 0, ItemRole::beam},
    {"weapon_plasmabeam", 0, ItemRole::beam},
    {"weapon_proxlauncher", 0, ItemRole::lobber},
    {"weapon_disintegrator", 0, ItemRole::launcher},
    {"ammo_flechettes", 0, ItemRole::ammo_rapid},
    {"ammo_prox", 0, ItemRole::ammo_lobber},
    {"ammo_disruptor", 0, ItemRole::ammo_explosive},
    {"item_double", 0, ItemRole::damage},
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
// Quake's stealth lays down as Quake II's adrenaline: Quake II has none.
inline constexpr ItemSpawn kQ2Spawns[] = {
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
    {"q1", kQ1Reads, kQ1Spawns},
    {"q2", kQ2Reads, kQ2Spawns},
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
//   does not have (Quake II's stimpacks and health boxes, its armour shards), the ones lying
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
    {"q1", "weapon_supershotgun", 1, "item_shells"},
    {"q1", "weapon_nailgun", 1, "item_spikes"},
    {"q1", "weapon_supernailgun", 2, "item_spikes"},
    {"q1", "weapon_grenadelauncher", 2, "item_rockets"},
    {"q1", "weapon_rocketlauncher", 3, "item_rockets"},
    {"q1", "weapon_lightning", 3, "item_cells"},
    {"q2", "weapon_shotgun", 1, "ammo_shells"},
    {"q2", "weapon_supershotgun", 1, "ammo_shells"},
    {"q2", "weapon_machinegun", 1, "ammo_bullets"},
    {"q2", "weapon_chaingun", 2, "ammo_bullets"},
    {"q2", "weapon_grenadelauncher", 2, "ammo_grenades"},
    {"q2", "weapon_hyperblaster", 2, "ammo_cells"},
    {"q2", "weapon_rocketlauncher", 3, "ammo_rockets"},
    {"q2", "weapon_railgun", 3, "ammo_slugs"},
    {"q2", "weapon_bfg", 4, "ammo_cells"},
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
    {"q1", "item_armor2", -1},       {"q1", "item_armorInv", -1},
    {"q1", "item_health", 2},  // the mega health
    {"q2", "item_armor_combat", -1}, {"q2", "item_armor_body", -1},
    {"q2", "item_power_shield", -1}, {"q2", "item_health_mega", -1},
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
    {"q2", "q1", "item_health_small", "health", 2},
    {"q2", "q1", "item_health", "health", 10},
    {"q2", "q1", "item_health_large", "health", 25},
    {"q2", "q1", "item_armor_shard", "armour", 2},
};
// Quake's health comes as fifteen (bit 1) and twenty-five; twenty shard points make a
// green armour.
inline constexpr PoolOutput kPoolOutputs[] = {
    {"q1", "health", "item_health", 0, 25},
    {"q1", "health", "item_health", 1, 15},
    {"q1", "armour", "item_armor1", 0, 20},
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
// artifacts 24 above their floor; Quake II's items are boxes 15 each way about their origin. A
// rewritten item keeps the middle of its floor where it was (`foot`: from the origin to that
// point), so an item dropped onto its floor does not start inside it. `kFootLift` raises the
// new place off the floor: a box resting on a plane counts as in it.
struct ItemFoot {
  std::string_view game;
  std::string_view class_name;  // a class, or a prefix ending in '_'; "" the game's other items
  float foot[3] = {0, 0, 0};
};

inline constexpr ItemFoot kItemFeet[] = {
    {"q1", "item_health", {16, 16, 0}},
    {"q1", "item_shells", {16, 16, 0}},
    {"q1", "item_spikes", {16, 16, 0}},
    {"q1", "item_rockets", {16, 16, 0}},
    {"q1", "item_cells", {16, 16, 0}},
    {"q1", "item_weapon", {16, 16, 0}},
    {"q1", "item_artifact_", {0, 0, -24}},
    {"q1", "", {0, 0, 0}},
    {"q2", "", {0, 0, -15}},
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

// THE MISSION PACKS' PART (`cr_expansions`): the items above that only a mission pack's maps
// lay down, and the weapons a played pack lays down in its base game's place, by role. With the
// setting off, a pack's items are not read (dropped, as any unknown item) and the played game's
// own spawns stand; with it on, a map remixed for a played pack lays the pack's weapon where its
// role has none of the base game's own (Quake's lightning gun stands in for both the rapid energy
// weapon and the room-clearer; Scourge of Armagon has its own).
struct PackItem {
  std::string_view game, pack, class_name;
};
inline constexpr PackItem kPackItems[] = {
    {"q1", "hipnotic", "weapon_laser_gun"},
    {"q1", "hipnotic", "weapon_mjolnir"},
    {"q1", "hipnotic", "weapon_proximity_gun"},
    {"q1", "hipnotic", "item_artifact_wetsuit"},
    {"q1", "rogue", "item_lava_spikes"},
    {"q1", "rogue", "item_multi_rockets"},
    {"q1", "rogue", "item_plasma"},
    {"q2", "rogue", "weapon_etf_rifle"},
    {"q2", "rogue", "weapon_heatbeam"},
    {"q2", "rogue", "weapon_plasmabeam"},
    {"q2", "rogue", "weapon_proxlauncher"},
    {"q2", "rogue", "weapon_disintegrator"},
    {"q2", "rogue", "ammo_flechettes"},
    {"q2", "rogue", "ammo_prox"},
    {"q2", "rogue", "ammo_disruptor"},
    {"q2", "rogue", "item_double"},
};
struct PackSpawn {
  std::string_view game, pack;
  ItemRole role;
  std::string_view class_name;
  int32_t bits = 0;
};
inline constexpr PackSpawn kPackSpawns[] = {
    {"q1", "hipnotic", ItemRole::energy, "weapon_laser_gun", 0},
    {"q1", "hipnotic", ItemRole::blast, "weapon_mjolnir", 0},
};

// What a remix may take from the mission packs: `on` the setting, `to_pack` the played game's
// pack folder (lower case; "" the base game).
struct RemixPacks {
  bool on = true;
  std::string_view to_pack;
};

// Whether `class_name` is an item only one of `game`'s mission packs lays down.
inline bool is_pack_item(std::string_view game, std::string_view class_name) {
  for (const PackItem& p : kPackItems)
    if (p.game == game && p.class_name == class_name) return true;
  return false;
}

// The weapon the played pack lays down for `class_name` read in `from`, or nullptr: only when the
// packs are on and the played pack has its own for that item's role.
inline const PackSpawn* pack_spawn(std::string_view from, std::string_view to,
                                   std::string_view class_name, int32_t spawnflags,
                                   const RemixPacks& packs);

// Whether `class_name` is an item in either game (`kItemPrefixes`).
inline bool is_item_class(std::string_view class_name) {
  for (std::string_view p : kItemPrefixes)
    if (class_name.substr(0, p.size()) == p) return true;
  return false;
}

// `class_name` with `spawnflags` read in `from` and laid down in `to` (`ge::classic::swap_item`);
// not known when either game has no list or `from` does not read it (an item then is dropped).
// With the packs off (`packs.on` false), a mission pack's item is not read.
inline ItemSwap remix_item(std::string_view from, std::string_view to, std::string_view class_name,
                           int32_t spawnflags, const RemixPacks& packs = {}) {
  const GameItems* f = game_items(from);
  const GameItems* t = game_items(to);
  if (f == nullptr || t == nullptr) return {};
  if (!packs.on && is_pack_item(from, class_name)) return {};
  return ge::classic::swap_item(*f, *t, class_name, spawnflags);
}

inline const PackSpawn* pack_spawn(std::string_view from, std::string_view to,
                                   std::string_view class_name, int32_t spawnflags,
                                   const RemixPacks& packs) {
  if (!packs.on || packs.to_pack.empty()) return nullptr;
  const GameItems* f = game_items(from);
  if (f == nullptr) return nullptr;
  const ge::classic::ItemReading* r = ge::classic::read_item(*f, class_name, spawnflags);
  if (r == nullptr) return nullptr;
  for (const PackSpawn& p : kPackSpawns)
    if (p.game == to && p.pack == packs.to_pack && p.role == r->role) return &p;
  return nullptr;
}

}  // namespace shooters_edge::profiles
