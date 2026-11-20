// ITEM ROLES: the part an item lying on a map plays, in words every shooter game shares, so one
// game's map items can become another game's set without a table for each pair of games. Each game
// lists its own items twice, in its own data:
//
// - **How it reads** (`ItemReading`): a class name, and the variant bits it needs (all of them), as
//   a role. Several readings may give one role (two shotguns, two kinds of energy cells); a class
//   may be read several ways by its variant bits, tried in order (a variant before the plain one).
// - **How it lays one down** (`ItemSpawn`): a role as a class name and the variant bits it takes.
//   A role the game has no spawn for is dropped: nothing in its set plays that part.
//
// `swap_item` reads an item in one game and lays it down in another: `known` false when the first
// game has no reading for it (an unknown item, dropped and counted by the caller), `becomes` empty
// when the second game has no spawn for its role. A third game adds its two lists and swaps with
// every game already here.
//
// The roles name what a thing does on a map, never a game's name for it: weapons by place in a
// set, ammunition by what spends it, health and armour by size, powerups by what they give.
//
// ge:summary Item roles: each game's map items read as shared roles and laid down as another's
// ge:keywords item-roles remix item-swap swap_item item_reading item_spawn by-role map-items
// ge:keywords spawnflags variant-bits dropped other-game
// ge:skill classic shooter
// ge:test tests/classic_item_roles_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ge::classic {

enum class ItemRole : uint8_t {
  // Weapons, by their place in a set.
  scatter,        // the first shotgun a map lays down
  scatter_heavy,  // the double-barrelled kind
  rapid,          // the light rapid-fire weapon
  rapid_heavy,    // the heavy rapid-fire weapon
  lobber,         // the grenade launcher
  launcher,       // the rocket launcher
  beam,           // the long-reaching hit-scan weapon (a beam or a rail)
  energy,         // the rapid energy weapon
  blast,          // the weapon that clears a room
  // Ammunition, by what spends it.
  ammo_scatter,
  ammo_scatter_large,  // a belt or a large box of shells
  ammo_rapid,
  ammo_lobber,
  ammo_explosive,
  ammo_pack,  // a pack of every kind
  ammo_beam,
  ammo_energy,
  // Health and armour, by size.
  health_small,
  health_large,
  health_mega,
  armour_shard,
  armour_light,
  armour_medium,
  armour_heavy,
  // Powerups, by what they give.
  damage,
  invulnerable,
  protect,  // from the hurt in liquids and air
  stealth,
  quiet,
};

// The spawnflags bits that say an item's variant; the bits above (skills, modes) are kept.
inline constexpr int32_t kVariantBits = 0xff;

struct ItemReading {
  std::string_view class_name;
  int32_t needs = 0;  // variant bits all present
  ItemRole role{};
};

struct ItemSpawn {
  ItemRole role{};
  std::string_view class_name;
  int32_t bits = 0;  // the variant bits it takes
};

// One game's items: its id (the content profile's) and its two lists.
struct GameItems {
  std::string_view game;
  std::span<const ItemReading> reads;
  std::span<const ItemSpawn> spawns;
};

struct ItemSwap {
  bool known = false;        // Quake reads the item
  std::string_view becomes;  // the second game's class; empty: dropped
  int32_t bits = 0;          // the variant bits it takes
};

// The first reading of `class_name` in `from` whose variant bits `spawnflags` holds, or nullptr.
inline const ItemReading* read_item(const GameItems& from, std::string_view class_name,
                                    int32_t spawnflags) {
  for (const ItemReading& r : from.reads)
    if (r.class_name == class_name && (spawnflags & r.needs & kVariantBits) == r.needs) return &r;
  return nullptr;
}

// The spawn of `role` in `to`, or nullptr.
inline const ItemSpawn* spawn_of(const GameItems& to, ItemRole role) {
  for (const ItemSpawn& s : to.spawns)
    if (s.role == role) return &s;
  return nullptr;
}

// `class_name` with `spawnflags` read in `from` and laid down in `to` (above).
inline ItemSwap swap_item(const GameItems& from, const GameItems& to, std::string_view class_name,
                          int32_t spawnflags) {
  ItemSwap out;
  const ItemReading* r = read_item(from, class_name, spawnflags);
  if (r == nullptr) return out;
  out.known = true;
  if (const ItemSpawn* s = spawn_of(to, r->role)) {
    out.becomes = s->class_name;
    out.bits = s->bits;
  }
  return out;
}

}  // namespace ge::classic
