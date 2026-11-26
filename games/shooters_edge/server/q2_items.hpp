// QUAKE II'S ITEMS AS DATA: every pickable thing on its maps (armour, power armour, weapons,
// ammo, health, powerups, keys) as one row of `kSqItems`, the player's body and inventory
// (`SqBody`), and the pickup rule each kind follows (`sq_pick_up`). The runner
// (q2_server.hpp) spawns an item from its class name, drops it to the floor, touches it with
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
// ge:summary Quake II's items as data: the table, the body and inventory, the pickup rule
// ge:keywords q2 items pickup armour armor health ammo weapon powerup key carry-limit
// ge:keywords inventory respawn sq_item sq_pick_up item-table armour-share mega-health
// ge:skill shooter
// ge:test tests/shooters_edge_q2_items_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "classic/q2_body.hpp"

#include "shooters_edge/server/q2_pack.hpp"

namespace shooters_edge::server {

enum class SqKind : uint8_t { armour, power_armour, weapon, ammo, health, powerup, key, pack };

// The ammo kinds, in the HUD's order; then each pack's (q2_pack.hpp): The Reckoning's mag slugs
// and traps, Ground Zero's flechettes, prox mines, teslas and the disruptor's rounds.
enum class SqAmmo : uint8_t {
  none,
  shells,
  bullets,
  grenades,
  rockets,
  cells,
  slugs,
  magslugs,
  traps,
  flechettes,
  prox,
  tesla,
  rounds
};
inline constexpr int kSqAmmoKinds = 13;  // `none` included, so an `SqAmmo` indexes an array

// The armours, worse to better (0 none).
enum class SqArmour : uint8_t { none, jacket, combat, body };

// The armours' counts and shares (plugins/classic/q2_body.hpp, which another game's rules wearing
// Quake II's armour read too).
using SqArmourRow = ge::classic::Q2ArmourRow;
inline constexpr const SqArmourRow (&kSqArmours)[4] = ge::classic::kQ2Armours;

struct SqItem {           // NOLINT(clang-analyzer-optin.performance.Padding): a table
  const char* classname;  // the map's name for it
  const char* pickup;     // the name a player reads
  const char* model;      // its world model
  const char* sound;      // the sound of taking it
  SqKind kind;
  int amount = 0;  // what it gives: armour's tier (SqArmour), ammo or health points, seconds
  SqAmmo ammo = SqAmmo::none;  // a weapon's or an ammo item's kind
  int respawn_s = 0;           // in deathmatch; 0 never
  // The rules it belongs to (q2_pack.hpp): the base game's rows first, then each pack's.
  Q2Pack pack = Q2Pack::base;
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
     SqKind::ammo, ge::classic::kQ2AmmoShells, SqAmmo::shells, 30},
    {"ammo_bullets", "Bullets", "models/items/ammo/bullets/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, ge::classic::kQ2AmmoBullets, SqAmmo::bullets, 30},
    {"ammo_cells", "Cells", "models/items/ammo/cells/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, ge::classic::kQ2AmmoCells, SqAmmo::cells, 30},
    {"ammo_rockets", "Rockets", "models/items/ammo/rockets/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, ge::classic::kQ2AmmoRockets, SqAmmo::rockets, 30},
    {"ammo_slugs", "Slugs", "models/items/ammo/slugs/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, ge::classic::kQ2AmmoSlugs, SqAmmo::slugs, 30},
    {"ammo_grenades", "Grenades", "models/items/ammo/grenades/medium/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, ge::classic::kQ2AmmoGrenades, SqAmmo::grenades, 30},
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
    // The Reckoning's (porting note: q2-reckoning-rules.md §§ 1, 3, 5, 8, 10, 11).
    {"weapon_boomer", "Ionripper", "models/weapons/g_boom/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 50, SqAmmo::cells, 30, Q2Pack::reckoning},
    {"weapon_phalanx", "Phalanx", "models/weapons/g_shotx/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 10, SqAmmo::magslugs, 30, Q2Pack::reckoning},
    {"ammo_magslug", "Mag Slug", "models/objects/ammo/tris.md2", "misc/am_pkup.wav", SqKind::ammo,
     10, SqAmmo::magslugs, 30, Q2Pack::reckoning},
    {"ammo_trap", "Trap", "models/weapons/g_trap/tris.md2", "misc/am_pkup.wav", SqKind::ammo, 1,
     SqAmmo::traps, 30, Q2Pack::reckoning},
    {"item_quadfire", "DualFire Damage", "models/items/quadfire/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 60, Q2Pack::reckoning},
    {"key_green_key", "Green Key", "models/items/keys/green_key/tris.md2", "items/pkup.wav",
     SqKind::key, 0, SqAmmo::none, 0, Q2Pack::reckoning},
    // What a fed trap leaves: health of its drop's count, past the maximum.
    {"foodcube", "Health", "models/objects/trapfx/tris.md2", "items/m_health.wav", SqKind::health,
     0, SqAmmo::none, 0, Q2Pack::reckoning},
    // Ground Zero's: its five weapons, their ammo, its power-ups and keys.
    {"weapon_etf_rifle", "ETF Rifle", "models/weapons/g_etf_rifle/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 50, SqAmmo::flechettes, 30, Q2Pack::ground_zero},
    {"weapon_proxlauncher", "Prox Launcher", "models/weapons/g_plaunch/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 5, SqAmmo::prox, 30, Q2Pack::ground_zero},
    {"weapon_plasmabeam", "Plasma Beam", "models/weapons/g_beamer/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 50, SqAmmo::cells, 30, Q2Pack::ground_zero},
    {"weapon_chainfist", "Chainfist", "models/weapons/g_chainf/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 0, SqAmmo::none, 30, Q2Pack::ground_zero},
    {"weapon_disintegrator", "Disruptor", "models/weapons/g_dist/tris.md2", "misc/w_pkup.wav",
     SqKind::weapon, 15, SqAmmo::rounds, 30, Q2Pack::ground_zero},
    {"ammo_flechettes", "Flechettes", "models/ammo/am_flechette/tris.md2", "misc/am_pkup.wav",
     SqKind::ammo, 50, SqAmmo::flechettes, 30, Q2Pack::ground_zero},
    {"ammo_prox", "Prox", "models/ammo/am_prox/tris.md2", "misc/am_pkup.wav", SqKind::ammo, 5,
     SqAmmo::prox, 30, Q2Pack::ground_zero},
    {"ammo_tesla", "Tesla", "models/ammo/am_tesl/tris.md2", "misc/am_pkup.wav", SqKind::ammo, 5,
     SqAmmo::tesla, 30, Q2Pack::ground_zero},
    {"ammo_nuke", "A-M Bomb", "models/weapons/g_nuke/tris.md2", "misc/am_pkup.wav", SqKind::powerup,
     300, SqAmmo::none, 300, Q2Pack::ground_zero},
    {"ammo_disruptor", "Rounds", "models/ammo/am_disr/tris.md2", "misc/am_pkup.wav", SqKind::ammo,
     15, SqAmmo::rounds, 30, Q2Pack::ground_zero},
    {"item_ir_goggles", "IR Goggles", "models/items/goggles/tris.md2", "items/pkup.wav",
     SqKind::powerup, 60, SqAmmo::none, 60, Q2Pack::ground_zero},
    {"item_double", "Double Damage", "models/items/ddamage/tris.md2", "items/pkup.wav",
     SqKind::powerup, 30, SqAmmo::none, 60, Q2Pack::ground_zero},
    {"item_compass", "compass", "models/objects/fire/tris.md2", "items/pkup.wav", SqKind::powerup,
     60, SqAmmo::none, 60, Q2Pack::ground_zero},
    {"item_sphere_vengeance", "vengeance sphere", "models/items/vengnce/tris.md2", "items/pkup.wav",
     SqKind::powerup, 60, SqAmmo::none, 60, Q2Pack::ground_zero},
    {"item_sphere_hunter", "hunter sphere", "models/items/hunter/tris.md2", "items/pkup.wav",
     SqKind::powerup, 120, SqAmmo::none, 120, Q2Pack::ground_zero},
    {"item_sphere_defender", "defender sphere", "models/items/defender/tris.md2", "items/pkup.wav",
     SqKind::powerup, 60, SqAmmo::none, 60, Q2Pack::ground_zero},
    {"item_doppleganger", "Doppleganger", "models/items/dopple/tris.md2", "items/pkup.wav",
     SqKind::powerup, 90, SqAmmo::none, 90, Q2Pack::ground_zero},
    {"dm_tag_token", "Tag Token", "models/items/tagtoken/tris.md2", "items/pkup.wav",
     SqKind::powerup, 0, SqAmmo::none, 0, Q2Pack::ground_zero},
    {"key_nuke_container", "Antimatter Pod", "models/weapons/g_nuke/tris.md2", "items/pkup.wav",
     SqKind::key, 0, SqAmmo::none, 0, Q2Pack::ground_zero},
    {"key_nuke", "Antimatter Bomb", "models/weapons/g_nuke/tris.md2", "items/pkup.wav", SqKind::key,
     0, SqAmmo::none, 0, Q2Pack::ground_zero},
};
inline constexpr int kSqItemCount = static_cast<int>(sizeof(kSqItems) / sizeof(kSqItems[0]));

// The row of `classname` among the rows `pack` plays: the pack's own row first (one may replace a
// base item), then the base game's; -1 when it is no item there.
int sq_item(std::string_view classname, Q2Pack pack = Q2Pack::base);

// Quake II's own effect bit that turns an entity about its yaw on the client's clock, 100° a
// second (an item lying about).
inline constexpr int kSqTurnEffect = 1;
// The effect bits a player's powers send (a client wears them as a blue and a red shell), and the
// render bit that pulses an item's light where it lies.
inline constexpr int kSqQuadEffect = 32768, kSqInvulnerableEffect = 65536;
// Ground Zero's double damage shell.
inline constexpr int kSqDoubleEffect = 0x08000000;
inline constexpr int kSqGlowRender = 512;
// A beam's render bits (a laser's): drawn from the origin to the row's far end, seen through.
inline constexpr int kSqBeamRender = 128, kSqTranslucentRender = 32;
// The effect bit that keeps specks rising over a teleporter's pad.
inline constexpr int kSqTeleporterEffect = 131072;
// The effect bit of flies buzzing round a corpse.
inline constexpr int kSqFliesEffect = 16384;
// A rocket's trail and glow on a thing that is not a rocket (the viper's bomb).
inline constexpr int kSqRocketEffect = 16;
// God mode's white shell: the colour-shell effect bit with the red, green and blue render bits.
inline constexpr int kSqColourShellEffect = 256, kSqGodShellRender = 1024 | 2048 | 4096;
// The red render bit alone: a red shell with the colour-shell effect.
inline constexpr int kSqRedShellRender = 1024;
// Power armour that has just taken a blow (for 0.2 s): a screen's green plate before its bearer
// (the effect bit), a shield's green shell (the colour shell with the green render bit).
inline constexpr int kSqPowerScreenEffect = 512, kSqShieldShellRender = 2048;
inline constexpr double kSqPowerFlashS = 0.2;
// The plate a power screen shows (the client draws it as a green shell).
inline constexpr const char* kSqPowerScreenModel = "models/items/armor/effect/tris.md2";
// Whether a power with `left` seconds still to run shows this server frame (10 a second): always
// while more than 3 s are left, then 4 frames on and 4 off.
inline bool sq_power_shown(double left) {
  if (left <= 0) return false;
  const int frames = static_cast<int>(left * 10);
  return frames > 30 || (frames & 4) != 0;
}
// Whether item `item` turns where it lies: armour, weapons, powerups, packs and keys, adrenaline
// and the ancient head; never ammo, the other health or the commander's head.
bool sq_turns(int item);

// A player's body and what he carries.
struct SqBody {
  int health = 100, max_health = 100;
  int counts[kSqItemCount] = {};     // how many of each item he holds (weapons, keys, powerups)
  int ammo[kSqAmmoKinds] = {};       // by `SqAmmo`
  int max_ammo[kSqAmmoKinds] = {};   // the carry limits
  SqArmour armour = SqArmour::none;  // the one worn
  int armour_count = 0;              // its points
  bool blaster = true;               // the starting weapon, always held
  // Quake's weapons worn (classic remix, q2_quake_arms.hpp): Quake's eight are held in place of
  // these rules' own, the axe and the shotgun always, and the items give Quake's.
  bool quake = false;
  double mega_until_drain = 0;  // level time a mega's count-down starts; 0 none
  int selected = 7;  // the selected item as the game's list numbers it (q2_bar.hpp); -1 none
  int cubes = 0;     // in coop, the bits of the power cubes taken (bit n: the level's cube n)
};

// A fresh body: health 100 of 100, the blaster, the starting carry limits (`pack`'s ammo too).
SqBody sq_new_body(Q2Pack pack = Q2Pack::base);

// Takes item row `item` into `body` by its kind's rule (`skill` 0..3; adrenaline raises the
// maximum only outside `deathmatch`); true when it is taken (the item hides or goes), false when
// the rule refuses it. `now` is the level time (the mega's count-down starts 5 s after).
bool sq_pick_up(int item, SqBody& body, bool deathmatch, int skill, double now, int count = 0);

}  // namespace shooters_edge::server
