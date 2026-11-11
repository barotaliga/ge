// THE SEQUEL'S ITEMS AS DATA: every pickable thing on its maps (armour, power armour, weapons,
// ammo, health, powerups, keys) as one row of `kSqItems`, the player's body and inventory
// (`SqBody`), and the pickup rule each kind follows (`sq_pick_up`). The runner
// (sequel_server.hpp) spawns an item from its class name, drops it to the floor, touches it with
// the players' boxes and hides it until its respawn; this file only says what an item is and
// what taking it does, so it is tested without a map.
//
// The numbers (amounts, carry limits, armour shares, respawn times) are the clean-room notes'
// (docs/goals/M18v.md, M18v7); the wording a pickup prints is our own.
//
// - **Ammo** is refused at the carry limit, else added up to it. A weapon is always taken: it adds
//   itself and its ammo's amount up to the limit; the grenades are both an ammo and a weapon.
// - **Health** is refused at the maximum unless the item goes past it (the small one, +2, and the
//   mega, +100, which then counts down to the maximum).
// - **Armour**: one worn at a time. With none, the new one's base; with one worn, the better
//   protection wins and the other's count is converted by the ratio of their shares, truncated,
//   held to the winner's maximum; a pickup that adds nothing is refused.
// - **Powerups**: refused when the skill's hold is full (2 held at skill 1, 1 at skill 2 or more).
// - **Keys** are always taken.
//
// ge:summary The sequel's items as data: the table, the body and inventory, the pickup rule
// ge:keywords sequel items pickup armour armor health ammo weapon powerup key carry-limit
// ge:keywords inventory respawn sq_item sq_pick_up kSqItems armour-share mega-health
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_items_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

namespace shooters_edge::server {

enum class SqKind : uint8_t { armour, power_armour, weapon, ammo, health, powerup, key, pack };

// The ammo kinds, in the HUD's order.
enum class SqAmmo : uint8_t { none, shells, bullets, grenades, rockets, cells, slugs };
inline constexpr int kSqAmmoKinds = 7;  // `none` included, so an `SqAmmo` indexes an array

// The armours, worse to better (0 none).
enum class SqArmour : uint8_t { none, jacket, combat, body };

struct SqArmourRow {
  int base, maximum;
  float normal, energy;  // the share of a blow it takes, of a normal hit and of an energy hit
};
inline constexpr SqArmourRow kSqArmours[] = {
    {0, 0, 0, 0},              // none
    {25, 50, 0.30f, 0.00f},    // jacket
    {50, 100, 0.60f, 0.30f},   // combat
    {100, 200, 0.80f, 0.60f},  // body
};

struct SqItem {
  const char* classname;  // the map's name for it
  const char* pickup;     // the name a player reads
  const char* model;      // its world model
  const char* sound;      // the sound of taking it
  SqKind kind;
  int amount = 0;  // what it gives: armour's tier (SqArmour), ammo or health points, seconds
  SqAmmo ammo = SqAmmo::none;  // a weapon's or an ammo item's kind
  int respawn_s = 0;           // in deathmatch; 0 never
};

// Every item, by class name. A key's row is last: `sq_item` finds any of them.
inline constexpr SqItem kSqItems[] = {
    {"item_armor_body", "Body Armor", "models/items/armor/body/tris.md2", "misc/ar1_pkup.wav",
     SqKind::armour, int(SqArmour::body), SqAmmo::none, 20},
    {"item_armor_combat", "Combat Armor", "models/items/armor/combat/tris.md2", "misc/ar1_pkup.wav",
     SqKind::armour, int(SqArmour::combat), SqAmmo::none, 20},
    {"item_armor_jacket", "Jacket Armor", "models/items/armor/jacket/tris.md2", "misc/ar1_pkup.wav",
     SqKind::armour, int(SqArmour::jacket), SqAmmo::none, 20},
    {"item_armor_shard", "Armor Shard", "models/items/armor/shard/tris.md2", "misc/ar2_pkup.wav",
     SqKind::armour, 0, SqAmmo::none, 20},
    {"item_power_screen", "Power Screen", "models/items/armor/screen/tris.md2", "misc/ar3_pkup.wav",
     SqKind::power_armour, 1, SqAmmo::none, 60},
    {"item_power_shield", "Power Shield", "models/items/armor/shield/tris.md2", "misc/ar3_pkup.wav",
     SqKind::power_armour, 2, SqAmmo::none, 60},
    {"weapon_shotgun", "Shotgun", "models/weapons/g_shotg/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 10, SqAmmo::shells, 30},
    {"weapon_supershotgun", "Super Shotgun", "models/weapons/g_shotg2/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 10, SqAmmo::shells, 30},
    {"weapon_machinegun", "Machinegun", "models/weapons/g_machn/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 50, SqAmmo::bullets, 30},
    {"weapon_chaingun", "Chaingun", "models/weapons/g_chain/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 50, SqAmmo::bullets, 30},
    {"weapon_grenadelauncher", "Grenade Launcher", "models/weapons/g_launch/tris.md2",
     "misc/w_pkup.wav", SqKind::weapon, 5, SqAmmo::grenades, 30},
    {"weapon_rocketlauncher", "Rocket Launcher", "models/weapons/g_rocket/tris.md2",
     "misc/w_pkup.wav", SqKind::weapon, 5, SqAmmo::rockets, 30},
    {"weapon_hyperblaster", "HyperBlaster", "models/weapons/g_hyperb/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 50, SqAmmo::cells, 30},
    {"weapon_railgun", "Railgun", "models/weapons/g_rail/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 10, SqAmmo::slugs, 30},
    {"weapon_bfg", "BFG10K", "models/weapons/g_bfg/tris.md2", "misc/w_pkup.wav", SqKind::weapon, 50,
     SqAmmo::cells, 30},
    {"ammo_shells", "Shells", "models/items/ammo/shells/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 10, SqAmmo::shells, 30},
    {"ammo_bullets", "Bullets", "models/items/ammo/bullets/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 50, SqAmmo::bullets, 30},
    {"ammo_cells", "Cells", "models/items/ammo/cells/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 50, SqAmmo::cells, 30},
    {"ammo_rockets", "Rockets", "models/items/ammo/rockets/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 5, SqAmmo::rockets, 30},
    {"ammo_slugs", "Slugs", "models/items/ammo/slugs/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 10, SqAmmo::slugs, 30},
    {"ammo_grenades", "Grenades", "models/items/ammo/grenades/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 5, SqAmmo::grenades, 30},
    {"item_health_small", "Health", "models/items/healing/stimpack/tris.md2", "items/s_health.wav",
     SqKind::health, 2, SqAmmo::none, 30},
    {"item_health", "Health", "models/items/healing/medium/tris.md2", "items/n_health.wav",
     SqKind::health, 10, SqAmmo::none, 30},
    {"item_health_large", "Health", "models/items/healing/large/tris.md2", "items/l_health.wav",
     SqKind::health, 25, SqAmmo::none, 30},
    {"item_health_mega", "Health", "models/items/mega_h/tris.md2", "items/m_health.wav",
     SqKind::health, 100, SqAmmo::none, 0},
    {"item_quad", "Quad Damage", "models/items/quaddama/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 60},
    {"item_invulnerability", "Invulnerability", "models/items/invulner/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 300},
    {"item_silencer", "Silencer", "models/items/silencer/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 60},
    {"item_breather", "Rebreather", "models/items/breather/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 60},
    {"item_enviro", "Environment Suit", "models/items/enviro/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 60},
    {"item_adrenaline", "Adrenaline", "models/items/adrenal/tris.md2", "items/pkup.wav",
     SqKind::health, 0, SqAmmo::none, 60},
    {"item_ancient_head", "Ancient Head", "models/items/c_head/tris.md2", "items/pkup.wav",
     SqKind::health, 0, SqAmmo::none, 60},
    {"item_bandolier", "Bandolier", "models/items/band/tris.md2", "items/pkup.wav", SqKind::pack, 1,
     SqAmmo::none, 60},
    {"item_pack", "Ammo Pack", "models/items/pack/tris.md2", "items/pkup.wav", SqKind::pack, 2,
     SqAmmo::none, 180},
    {"key_blue_key", "Blue Key", "models/items/keys/key/tris.md2", "items/pkup.wav", SqKind::key},
    {"key_red_key", "Red Key", "models/items/keys/red_key/tris.md2", "items/pkup.wav", SqKind::key},
    {"key_data_cd", "Data CD", "models/items/keys/data_cd/tris.md2", "items/pkup.wav", SqKind::key},
    {"key_power_cube", "Power Cube", "models/items/keys/power/tris.md2", "items/pkup.wav",
     SqKind::key},
    {"key_pyramid", "Pyramid Key", "models/items/keys/pyramid/tris.md2", "items/pkup.wav",
     SqKind::key},
    {"key_data_spinner", "Data Spinner", "models/items/keys/spinner/tris.md2", "items/pkup.wav",
     SqKind::key},
    {"key_pass", "Security Pass", "models/items/keys/pass/tris.md2", "items/pkup.wav", SqKind::key},
    {"key_commander_head", "Commander's Head", "models/monsters/commandr/head/tris.md2",
     "items/pkup.wav", SqKind::key},
    {"key_airstrike_target", "Airstrike Marker", "models/items/keys/target/tris.md2",
     "items/pkup.wav", SqKind::key},
};
inline constexpr int kSqItemCount = static_cast<int>(sizeof(kSqItems) / sizeof(kSqItems[0]));

// The row of `classname`, or -1 when it is no item.
int sq_item(std::string_view classname);

// A player's body and what he carries.
struct SqBody {
  int health = 100, max_health = 100;
  int counts[kSqItemCount] = {};     // how many of each item he holds (weapons, keys, powerups)
  int ammo[kSqAmmoKinds] = {};       // by `SqAmmo`
  int max_ammo[kSqAmmoKinds] = {};   // the carry limits
  SqArmour armour = SqArmour::none;  // the one worn
  int armour_count = 0;              // its points
  bool blaster = true;               // the starting weapon, always held
  double mega_until_drain = 0;       // level time a mega's count-down starts; 0 none
};

// A fresh body: health 100 of 100, the blaster, the starting carry limits.
SqBody sq_new_body();

// Takes item row `item` into `body` by its kind's rule (`skill` 0..3; adrenaline raises the
// maximum only outside `deathmatch`); true when it is taken (the item hides or goes), false when
// the rule refuses it. `now` is the level time (the mega's count-down starts 5 s after).
bool sq_pick_up(int item, SqBody& body, bool deathmatch, int skill, double now);

}  // namespace shooters_edge::server
