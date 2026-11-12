// THE SEQUEL'S WEAPONS AND DAMAGE AS DATA: every weapon a player holds (`kSqWeapons`: its frames,
// its ammo, its shot), the hand that steps a weapon's frames once a rules frame (`SqHand`,
// `sq_hand_step`), the best weapon with ammo, and how a blow is shared between armour and health
// (`sq_armour_save`, `sq_knockback`). The runner (sequel_fight.cpp) makes the shots the step asks
// for, moves the missiles and deals the damage; this file only says what a weapon is and when it
// fires, so it is tested without a map.
//
// The numbers (frames, damage, spreads, speeds) are the clean-room notes' (docs/goals/M18v.md,
// M18v7b); the words a player reads are our own.
//
// - **The step** runs once a rules frame (100 ms) through activating, ready, firing and
//   dropping. Ready with attack and enough ammo fires from the first fire frame in the same step;
//   without, a click (at most once a second) and the best weapon with ammo is chosen. A firing
//   weapon is ready again a step after its last fire frame, so its period is (last fire frame −
//   last activate frame + 1) steps.
// - **Held fire**: the machinegun alternates frames 4 and 5 while attack is held; the chaingun
//   loops 15 to 21 (1, 2, then 3 bullets a step) and stops early at 14 on release; the
//   hyperblaster loops 12 back to 6.
// - **Armour** saves the share of a blow (the energy share for energy hits), rounded up, at most
//   its count; **knockback** pushes by 500 × knockback / mass (1600 when a player hits himself).
//
// ge:summary The sequel's weapons as data: the frames, the hand's step, the best weapon, armour
// ge:keywords sequel weapons weapon-frames hand-step fire-frame period ammo-per-shot best-weapon
// ge:keywords armour-save knockback rocket-jump machinegun chaingun hyperblaster railgun bfg
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_combat_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "shooters_edge/server/sequel_items.hpp"

namespace shooters_edge::server {

// The weapons a player can hold, in the order a number key picks them (1 the blaster).
enum class SqWeapon : uint8_t {
  blaster,
  shotgun,
  super_shotgun,
  machinegun,
  chaingun,
  grenade_launcher,
  rocket_launcher,
  hyperblaster,
  railgun,
  bfg,
};
inline constexpr int kSqWeaponCount = 10;

// What one shot is.
enum class SqShot : uint8_t { bolt, pellets, bullet, grenade, rocket, rail, bfg };

struct SqWeaponRow {
  const char* name;        // the name a player reads and `use` takes
  const char* item;        // the item that gives it (sequel_items.hpp); null for the blaster
  const char* view_model;  // the first-person model
  const char* sound;       // its firing sound
  SqAmmo ammo;
  int per_shot;  // ammo a shot takes (0 the blaster)
  // The last frame of each run: activate 0..a, fire a+1..f, idle f+1..i, deactivate i+1..d.
  int last_activate, last_fire, last_idle, last_deactivate;
  int fire_from, fire_to;  // the frames that fire (a range; the held-fire weapons loop in it)
  SqShot shot;
  int damage_dm, damage_sp;  // a hit's damage, in deathmatch and outside it
  int kick;                  // a hit's knockback
  float forward, right;      // the shot's start from the eye, units
};

inline constexpr SqWeaponRow kSqWeapons[kSqWeaponCount] = {
    {"Blaster", nullptr, "models/weapons/v_blast/tris.md2", "weapons/blastf1a.wav", SqAmmo::none, 0,
     4, 8, 52, 55, 5, 5, SqShot::bolt, 15, 10, 1, 24, 8},
    {"Shotgun", "weapon_shotgun", "models/weapons/v_shotg/tris.md2", "weapons/shotgf1b.wav",
     SqAmmo::shells, 1, 7, 18, 36, 39, 8, 8, SqShot::pellets, 4, 4, 8, 8, 8},
    {"Super Shotgun", "weapon_supershotgun", "models/weapons/v_shotg2/tris.md2",
     "weapons/sshotf1b.wav", SqAmmo::shells, 2, 6, 17, 57, 61, 7, 7, SqShot::pellets, 6, 6, 12, 8,
     8},
    {"Machinegun", "weapon_machinegun", "models/weapons/v_machn/tris.md2", "weapons/machgf1b.wav",
     SqAmmo::bullets, 1, 3, 5, 45, 49, 4, 5, SqShot::bullet, 8, 8, 2, 8, 8},
    {"Chaingun", "weapon_chaingun", "models/weapons/v_chain/tris.md2", "weapons/machgf2b.wav",
     SqAmmo::bullets, 1, 4, 31, 61, 64, 5, 21, SqShot::bullet, 6, 8, 2, 8, 7},
    {"Grenade Launcher", "weapon_grenadelauncher", "models/weapons/v_launch/tris.md2",
     "weapons/grenlf1a.wav", SqAmmo::grenades, 1, 5, 16, 59, 64, 6, 6, SqShot::grenade, 120, 120, 0,
     8, 8},
    {"Rocket Launcher", "weapon_rocketlauncher", "models/weapons/v_rocket/tris.md2",
     "weapons/rocklf1a.wav", SqAmmo::rockets, 1, 4, 12, 50, 54, 5, 5, SqShot::rocket, 100, 100, 0,
     8, 8},
    {"HyperBlaster", "weapon_hyperblaster", "models/weapons/v_hyperb/tris.md2",
     "weapons/hyprbf1a.wav", SqAmmo::cells, 1, 5, 20, 49, 53, 6, 11, SqShot::bolt, 15, 20, 1, 24,
     8},
    {"Railgun", "weapon_railgun", "models/weapons/v_rail/tris.md2", "weapons/railgf1a.wav",
     SqAmmo::slugs, 1, 3, 18, 56, 61, 4, 4, SqShot::rail, 100, 150, 200, 0, 7},
    {"BFG10K", "weapon_bfg", "models/weapons/v_bfg/tris.md2", "weapons/bfg__f1y.wav", SqAmmo::cells,
     50, 8, 32, 55, 58, 17, 17, SqShot::bfg, 200, 500, 0, 8, 8},
};

// The weapon `item` (an item row) gives, or -1 when it gives none.
int sq_weapon_of_item(int item);
// The weapon called `name` (its name, any case), or -1.
int sq_weapon_named(std::string_view name);

// The item a player calls `name` (its pickup name, any case), or -1.
int sq_item_named(std::string_view name);

// Whether `body` holds weapon `w` (the blaster always).
bool sq_holds(const SqBody& body, int w);
// Whether `body` holds `w` and the ammo for a shot of it.
bool sq_can_fire(const SqBody& body, int w);
// The best weapon with ammo: railgun, hyperblaster, chaingun, machinegun, super shotgun (2 shells
// or more), shotgun, blaster.
int sq_best_weapon(const SqBody& body);

enum class SqHandState : uint8_t { activating, ready, firing, dropping };

// The weapon in a player's hand and its view model's frame.
struct SqHand {
  int weapon = int(SqWeapon::blaster);
  int pending = -1;  // the weapon chosen next; -1 none
  SqHandState state = SqHandState::activating;
  int frame = 0;
  double click_at = -10;  // level time of the last no-ammo click
  int held_shots = 0;     // the machinegun's shots in a row (its climbing kick)
};

// What a step asks of the runner.
struct SqStep {
  int shots = 0;         // shots of the hand's weapon to make this step (ammo already taken)
  bool click = false;    // the no-ammo click
  bool changed = false;  // the weapon in hand changed (its view model)
};

// Steps the hand once (`attack` held this frame, `now` the level time); ammo is taken from `body`
// for each shot it asks for. `infinite`: shots take no ammo.
SqStep sq_hand_step(SqHand& hand, SqBody& body, bool attack, double now, bool infinite = false);

// Chooses weapon `w` for the hand (when held and not already in hand); true when chosen.
bool sq_choose(SqHand& hand, const SqBody& body, int w);

// The armour's part of a blow of `damage` (taken off its count): the share (the energy share for
// an energy hit) × damage, rounded up, at most the count.
int sq_armour_save(SqBody& body, int damage, bool energy);

// The speed a blow adds along its (normalised) way: 500 × knockback / mass (at least 50), or 1600 ×
// when a player hits himself (the rocket jump).
float sq_knockback(int knockback, float mass, bool self);

}  // namespace shooters_edge::server
