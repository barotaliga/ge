// QUAKE II'S WEAPONS AND DAMAGE AS DATA: every weapon a player holds (`kSqWeapons`: its frames,
// its ammo, its shot), the hand that steps a weapon's frames once a rules frame (`SqHand`,
// `sq_hand_step`), the best weapon with ammo, and how a blow is shared between armour and health
// (`sq_armour_save`, `sq_knockback`). The runner (q2_fight.cpp) makes the shots the step asks
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
// - **The hand grenade** steps 1 to 15: its fuse (3.2 s) is lit at 11, which holds while attack is
//   held; let go, it is thrown at 400 + (3 − fuse left) × 400 / 3 units/s (400 to 800), and held
//   to the fuse's end it explodes in the hand. Frame 15 waits for the fuse to end; 16 is ready.
// - **Armour** saves the share of a blow (the energy share for energy hits), rounded up, at most
//   its count; **knockback** pushes by 500 × knockback / mass (1600 when a player hits himself).
//
// ge:summary Quake II's weapons as data: the frames, the hand's step, the best weapon, armour
// ge:keywords q2 weapons weapon-frames hand-step fire-frame period ammo-per-shot best-weapon
// ge:keywords armour-save knockback rocket-jump machinegun chaingun hyperblaster railgun bfg
// ge:skill shooter
// ge:test tests/shooters_edge_q2_combat_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "shooter/vec.hpp"
#include "shooters_edge/server/q2_items.hpp"

namespace shooters_edge::server {

// The weapons a player can hold, in the order a number key picks them (1 the blaster); the hand
// grenade, last, is chosen by `use Grenades`.
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
  hand_grenade,
  // The Reckoning's (q2_pack.hpp): the ion ripper, the phalanx and the trap (thrown as the hand
  // grenade is).
  ion_ripper,
  phalanx,
  trap,
  // Ground Zero's (q2_gz.hpp): the chainfist, the ETF rifle, the prox launcher, the plasma beam,
  // the disruptor and the thrown tesla.
  chainfist,
  etf_rifle,
  prox_launcher,
  plasma_beam,
  disruptor,
  tesla,
};
inline constexpr int kSqWeaponCount = 20;

// What one shot is. The Reckoning's after the base game's: the ion ripper's bouncing bolt, the
// hyper guard's blue bolt, the phalanx's plasma ball, the beta maiden's homing rocket, the gekk's
// spit and the thrown trap.
enum class SqShot : uint8_t {
  bolt,
  pellets,
  bullet,
  grenade,
  rocket,
  rail,
  bfg,
  hand_grenade,
  ripper,
  blue_bolt,
  plasma,
  heat_rocket,
  spit,
  trap,
  // Ground Zero's: the flechette, the prox mine, the tesla, the disruptor's bolt, the A-M bomb, and
  // the two that never fly (the plasma beam's line, the chainfist's reach).
  flechette,
  prox,
  tesla,
  tracker,
  nuke,
  heat_beam,
  chainfist,
};
inline constexpr int kSqShotKinds = 21;
// Each kind's effect bits in flight (its trail and light on a client): a bolt the blaster's (8), a
// grenade's smoke (32), a rocket's (16), the BFG's ball (128) with its frames run fast (0x2000);
// the ion ripper's trail (0x00100000), the blue bolt's (0x00400000), the plasma's (0x01000000,
// its frames fast), the spit the blaster's; Ground Zero's mines and bomb the grenade's smoke, the
// disruptor's bolt the tracker's (0x04000000).
inline constexpr int kSqShotEffects[kSqShotKinds] = {
    8,  0, 0, 32, 16, 0,  128 | 0x2000, 32, 0x00100000, 0x00400000, 0x01000000 | 0x2000,
    16, 8, 0, 0,  32, 32, 0x04000000,   32, 0,          0};
// The render flag each kind is drawn with (8 full bright: the ion ripper's bolt, the flechette;
// 0x8000 seen through IR goggles: Ground Zero's mines and bomb).
inline constexpr int kSqShotRender[kSqShotKinds] = {0, 0, 0, 0, 0,      0,      0, 0,      8, 0, 0,
                                                    0, 0, 0, 8, 0x8000, 0x8000, 0, 0x8000, 0, 0};

struct SqWeaponRow {       // NOLINT(clang-analyzer-optin.performance.Padding): a table
  const char* name;        // the name a player reads and `use` takes
  const char* item;        // the item that gives it (q2_items.hpp); null for the blaster
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
  // The rules it belongs to (q2_pack.hpp): its view model, sound and shots are registered only
  // under its pack, so the base game's lists do not move.
  Q2Pack pack = Q2Pack::base;
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
    {"Grenades", "ammo_grenades", "models/weapons/v_handgr/tris.md2", "weapons/hgrent1a.wav",
     SqAmmo::grenades, 1, 0, 15, 48, 48, 11, 12, SqShot::hand_grenade, 125, 125, 0, 8, 8},
    // The Reckoning's (porting note: q2-reckoning-rules.md §§ 1 to 5). The phalanx takes its mag
    // slug with the second ball (a pull's), the trap steps as the hand grenade does.
    {"Ionripper", "weapon_boomer", "models/weapons/v_boomer/tris.md2", "weapons/rippfire.wav",
     SqAmmo::cells, 2, 4, 6, 36, 39, 5, 5, SqShot::ripper, 30, 50, 1, 16, 7, Q2Pack::reckoning},
    {"Phalanx", "weapon_phalanx", "models/weapons/v_shotx/tris.md2", "weapons/plasshot.wav",
     SqAmmo::magslugs, 1, 5, 20, 58, 63, 7, 8, SqShot::plasma, 70, 70, 0, 0, 8, Q2Pack::reckoning},
    {"Trap", "ammo_trap", "models/weapons/v_trap/tris.md2", "weapons/trapcock.wav", SqAmmo::traps,
     1, 0, 15, 48, 48, 11, 12, SqShot::trap, 125, 125, 0, 8, 8, Q2Pack::reckoning},
    // Ground Zero's (q2_gz.hpp). The chainfist swings from frame 6 (its hits on 8, 9, 16 to 18, 30
    // and 31), the ETF rifle and the plasma beam loop while held, the tesla is held on frame 1 and
    // thrown on 2; their firing sounds are their muzzle flashes'.
    {"Chainfist", "weapon_chainfist", "models/weapons/v_chainf/tris.md2", "weapons/sawhit.wav",
     SqAmmo::none, 0, 4, 32, 57, 60, 6, 31, SqShot::chainfist, 30, 15, 0, 0, 8,
     Q2Pack::ground_zero},
    {"ETF Rifle", "weapon_etf_rifle", "models/weapons/v_etf_rifle/tris.md2", "weapons/nail1.wav",
     SqAmmo::flechettes, 1, 4, 7, 37, 41, 6, 7, SqShot::flechette, 10, 10, 3, 15, 8,
     Q2Pack::ground_zero},
    {"Prox Launcher", "weapon_proxlauncher", "models/weapons/v_plaunch/tris.md2",
     "weapons/grenlf1a.wav", SqAmmo::prox, 1, 5, 16, 59, 64, 6, 6, SqShot::prox, 90, 90, 0, 8, 8,
     Q2Pack::ground_zero},
    {"Plasma Beam", "weapon_plasmabeam", "models/weapons/v_beamer/tris.md2", "weapons/bfg__l1a.wav",
     SqAmmo::cells, 2, 8, 12, 39, 44, 9, 12, SqShot::heat_beam, 15, 15, 75, 7, 2,
     Q2Pack::ground_zero},
    {"Disruptor", "weapon_disintegrator", "models/weapons/v_dist/tris.md2", "weapons/disrupt.wav",
     SqAmmo::rounds, 1, 4, 9, 29, 34, 5, 5, SqShot::tracker, 30, 45, 0, 24, 8, Q2Pack::ground_zero},
    {"Tesla", "ammo_tesla", "models/weapons/v_tesla/tris.md2", "weapons/hgrent1a.wav",
     SqAmmo::tesla, 1, 0, 8, 32, 32, 1, 2, SqShot::tesla, 3, 3, 8, 0, -4, Q2Pack::ground_zero},
};

// The pack each shot kind belongs to, by `SqShot` (its missile model registered only under it).
inline constexpr Q2Pack kSqShotPacks[kSqShotKinds] = {
    Q2Pack::base,        Q2Pack::base,        Q2Pack::base,        Q2Pack::base,
    Q2Pack::base,        Q2Pack::base,        Q2Pack::base,        Q2Pack::base,
    Q2Pack::reckoning,   Q2Pack::reckoning,   Q2Pack::reckoning,   Q2Pack::reckoning,
    Q2Pack::reckoning,   Q2Pack::reckoning,   Q2Pack::ground_zero, Q2Pack::ground_zero,
    Q2Pack::ground_zero, Q2Pack::ground_zero, Q2Pack::ground_zero, Q2Pack::ground_zero,
    Q2Pack::ground_zero};
// Whether a weapon steps and throws as the hand grenade does (the grenades and the trap).
constexpr bool sq_thrown(int w) {
  return w == int(SqWeapon::hand_grenade) || w == int(SqWeapon::trap);
}
// Whether weapon `w` plays under `pack` (the base game's always, a pack's under it).
constexpr bool sq_weapon_plays(int w, Q2Pack pack) {
  return w >= 0 && w < kSqWeaponCount && q2_pack_has(pack, kSqWeapons[w].pack);
}

// The hand grenade's fuse, s, lit at frame 11; its throw's speed runs from the least (let go at
// once) to the most (held 3 s), units/s.
inline constexpr double kSqGrenadeFuse = 3.2;
inline constexpr float kSqThrowLeast = 400, kSqThrowMost = 800;

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
  double fuse_at = 0;     // the hand grenade's: level time its fuse ends; 0 not lit
  float throw_speed = 0;  // Ground Zero's tesla: the speed of the throw this step asks for
  int swing = 0;          // Ground Zero's chainfist: the swing played last (0 A, 1 B, 2 C)
};

// What a step asks of the runner.
struct SqStep {
  int shots = 0;         // shots of the hand's weapon to make this step (ammo already taken)
  bool click = false;    // the no-ammo click
  bool changed = false;  // the weapon in hand changed (its view model)
  // The hand grenade's: its fuse lit (the ticking), and a shot's throw speed and fuse left (a shot
  // with no speed is the grenade held to the end of its fuse: it explodes in the hand).
  bool lit = false;
  float throw_speed = 0;
  double fuse_left = 0;
};

// Steps the hand once (`attack` held this frame, `now` the level time); ammo is taken from `body`
// for each shot it asks for. `infinite`: shots take no ammo. `swap`: the frames a lower or a raise
// advances a step (g_swap_speed; 1 the classic pace).
SqStep sq_hand_step(SqHand& hand, SqBody& body, bool attack, double now, bool infinite = false,
                    int swap = 1);

// A match server's faster weapon switch (`g_fast_weap_switch`), before the hand's step: `mode` 1
// skips the lowering (the swap is this step's), 2 also the raising (ready the step after the swap);
// 0 leaves the classic pace.
void sq_hand_hasten(SqHand& hand, int mode);

// Chooses weapon `w` for the hand (when held and not already in hand); true when chosen.
bool sq_choose(SqHand& hand, const SqBody& body, int w);

// The muzzle flash value (server/q2_flash.hpp) a shot of weapon `w` sends, `shots` the chain gun's
// bullets this frame; -1 for a weapon with none (the hand grenade). The server sends it and the
// client's weapon prediction plays it (net/q2_weapon.hpp).
int sq_flash_value(int w, int shots);
// Whether flash `value` plays `sample` as its shot (a remixed weapon's own sound is sent as a sound
// instead).
bool sq_flash_plays(int value, const char* sample);

// THE HAND'S OWN SOUNDS, the server's and the joined window's prediction alike: the one-shots a
// step's frames make (the hand grenade's pin, the chaingun's spin up and down, the hyperblaster's
// wind-down) and the loop the hand carries (the railgun's and the BFG's hum while held, the
// hyperblaster's and the chaingun's spin while firing, a lit hand grenade's fuse in the hand).
struct SqHandSound {
  const char* sample;
  int channel;      // 0 any, 1 the weapon's
  int attenuation;  // the protocol's factor
};
inline constexpr int kSqHandSounds = 5;
inline constexpr SqHandSound kSqHandSound[kSqHandSounds] = {{"weapons/hgrena1b.wav", 1, 1},
                                                            {"weapons/chngnu1a.wav", 0, 2},
                                                            {"weapons/chngnd1a.wav", 0, 2},
                                                            {"weapons/hyprbd1a.wav", 0, 1},
                                                            {"weapons/trapcock.wav", 1, 1}};
// The one-shot (kSqHandSound's row) the step that took weapon `weapon`'s hand from frame `was`
// (firing or not) to `after` makes; -1 none. A weapon changed by the step makes none.
int sq_hand_sound(int weapon, int was, bool was_firing, const SqHand& after);
// The loop `h` carries (a sample's name), null none.
const char* sq_hand_loop(const SqHand& h);

// The armour's part of a blow of `damage` (taken off its count): the share (the energy share for
// an energy hit) × damage, rounded up, at most the count.
int sq_armour_save(SqBody& body, int damage, bool energy);

// The speed a blow adds along its (normalised) way: 500 × knockback / mass (at least 50), or 1600 ×
// when a player hits himself (the rocket jump).
float sq_knockback(int knockback, float mass, bool self);

// The entry of the segment `from`..`to` into the box `lo`..`hi`, 0..1, or -1 when it misses (0
// when it starts inside).
float sq_enter_box(ge::shooter::Vec3f from, ge::shooter::Vec3f to, ge::shooter::Vec3f lo,
                   ge::shooter::Vec3f hi);

}  // namespace shooters_edge::server
