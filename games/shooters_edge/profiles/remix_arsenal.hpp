// CLASSIC REMIX'S ARSENALS: a game's rules dressed in another game's things (`cr_items`,
// docs/goals/M18u.md). The played game's rules keep their own numbers (a weapon's damage, its
// rate, an armour's share) and their own class names on the map, so the map plays as it always
// has; what a player sees and hears is the other game's: each weapon's view model and pickup, its
// missile, its fire, bounce and blast sounds, and each armour's model and pickup sound.
//
// - **By role** (`kRemixArsenals`): the weapon in a slot takes the other game's weapon of the
//   same place, as the item table does (profiles/remix.hpp: the nail weapons the bullet ones, the
//   beam the rail), the three armours the other game's three, weakest first, and the ammunition
//   and health boxes the other game's of the same kind and size (`ItemLook`).
// - **The held set** (`RemixArsenal::held`): the view models by slot, which the window draws in
//   the hand in place of the game's own.
// - **The frames** (`HeldFrames`, `held_frames`): a view model of the sequel's runs every
//   animation in one file (up, firing, idle, down); the hand takes the first idle frame as its
//   rest and the firing frames as its shot.
//
// ge:summary Classic remix's arsenals: a game's rules with another game's weapons, armour, sounds
// ge:keywords remix classic-remix cr_items remix-arsenal held-frames sequel-weapons
// ge:keywords view-model pickup-model fire-sound by-role remix_arsenal held_frames
// ge:skill shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "shooters_edge/profiles/arsenal.hpp"

namespace shooters_edge::profiles {

// How far above the floor the sequel's items stand: its items are measured from their middle,
// 15 units over the floor (profiles/remix.hpp's `kItemFeet`).
inline constexpr int32_t kSequelLift = 15;

// The classic game's eight weapons, their numbers its own, as the sequel's: the axe's slot the
// blaster, the nailguns the machinegun and chaingun, the lightning gun the railgun.
inline constexpr WeaponRow kClassicAsSequelWeapons[] = {
    {1, Fire::melee, Ammo::none, 0, 0.5f, 1, 20, 0, 64, 0, 0, "models/weapons/v_blast/tris.md2",
     "weapons/blastf1a.wav", "-", "-", "-", "-", 0},
    {2, Fire::pellets, Ammo::shells, 1, 0.5f, 6, 4, 0.04f, 2048, 0, 0,
     "models/weapons/v_shotg/tris.md2", "weapons/shotgf1b.wav", "-", "-", "-", "-", 0},
    {3, Fire::pellets, Ammo::shells, 2, 0.7f, 14, 4, 0.07f, 2048, 0, 0,
     "models/weapons/v_shotg2/tris.md2", "weapons/sshotf1b.wav", "-", "-", "-",
     "models/weapons/g_shotg2/tris.md2", 5},
    {4, Fire::spike, Ammo::nails, 1, 0.1f, 1, 9, 0, 1000, 6, 0, "models/weapons/v_machn/tris.md2",
     "weapons/machgf1b.wav", "models/objects/laser/tris.md2", "-", "-",
     "models/weapons/g_machn/tris.md2", 30},
    {5, Fire::spike, Ammo::nails, 2, 0.1f, 1, 18, 0, 1000, 6, 0, "models/weapons/v_chain/tris.md2",
     "weapons/machgf2b.wav", "models/objects/laser/tris.md2", "-", "-",
     "models/weapons/g_chain/tris.md2", 30},
    {6, Fire::grenade, Ammo::rockets, 1, 0.6f, 1, 0, 0, 600, 2.5f, 120,
     "models/weapons/v_launch/tris.md2", "weapons/grenlf1a.wav", "models/objects/grenade/tris.md2",
     "weapons/grenlb1b.wav", "weapons/grenlx1a.wav", "models/weapons/g_launch/tris.md2", 5},
    {7, Fire::rocket, Ammo::rockets, 1, 0.8f, 1, 100, 0, 1000, 5, 120,
     "models/weapons/v_rocket/tris.md2", "weapons/rocklf1a.wav", "models/objects/rocket/tris.md2",
     "-", "weapons/rocklx1a.wav", "models/weapons/g_rocket/tris.md2", 5},
    {8, Fire::beam, Ammo::cells, 1, 0.1f, 1, 30, 0, 600, 0, 35, "models/weapons/v_rail/tris.md2",
     "weapons/railgf1a.wav", "-", "-", "-", "models/weapons/g_rail/tris.md2", 15},
};
// Its three armours as the sequel's jacket, combat and body armour.
inline constexpr ArmourRow kClassicAsSequelArmour[] = {
    {1, 0.3f, 100, 0, "models/items/armor/jacket/tris.md2", "misc/ar1_pkup.wav"},
    {2, 0.6f, 150, 0, "models/items/armor/combat/tris.md2", "misc/ar1_pkup.wav"},
    {3, 0.8f, 200, 0, "models/items/armor/body/tris.md2", "misc/ar1_pkup.wav"},
};
// Its ammunition and health as the sequel's: the nails the bullets, the cells the slugs, the
// small health the medium, the plain one the large, the mega health the sequel's own.
inline constexpr ItemLook kClassicAsSequelLooks[] = {
    {"arsenal_ammo1", "models/items/ammo/shells/medium/tris.md2", "misc/am_pkup.wav", kSequelLift},
    {"arsenal_ammo2", "models/items/ammo/bullets/medium/tris.md2", "misc/am_pkup.wav", kSequelLift},
    {"arsenal_ammo3", "models/items/ammo/rockets/medium/tris.md2", "misc/am_pkup.wav", kSequelLift},
    {"arsenal_ammo4", "models/items/ammo/slugs/medium/tris.md2", "misc/am_pkup.wav", kSequelLift},
    {"arsenal_health1", "models/items/healing/medium/tris.md2", "items/n_health.wav", kSequelLift},
    {"arsenal_health2", "models/items/healing/large/tris.md2", "items/l_health.wav", kSequelLift},
    {"arsenal_health3", "models/items/mega_h/tris.md2", "items/m_health.wav", kSequelLift},
};
// The view models by slot, 1..8.
inline constexpr std::string_view kClassicAsSequelHeld[] = {
    "models/weapons/v_blast/tris.md2",  "models/weapons/v_shotg/tris.md2",
    "models/weapons/v_shotg2/tris.md2", "models/weapons/v_machn/tris.md2",
    "models/weapons/v_chain/tris.md2",  "models/weapons/v_launch/tris.md2",
    "models/weapons/v_rocket/tris.md2", "models/weapons/v_rail/tris.md2"};

struct RemixArsenal {
  std::string_view played;  // the content profile id whose rules play
  std::string_view items;   // the content profile id whose things they wear
  Arsenal arsenal;
  std::span<const std::string_view> held;  // the view models by slot, 1 first
};

inline constexpr RemixArsenal kRemixArsenals[] = {
    {"classic",
     "sequel",
     {kClassicAsSequelWeapons, kClassicAsSequelArmour, kClassicAsSequelLooks, "misc/w_pkup.wav"},
     kClassicAsSequelHeld},
};

// The arsenal `played`'s rules wear with `items`'s things, or null (the same game, or no row).
constexpr const RemixArsenal* remix_arsenal(std::string_view played, std::string_view items) {
  if (played == items) return nullptr;
  for (const RemixArsenal& r : kRemixArsenals)
    if (r.played == played && r.items == items) return &r;
  return nullptr;
}

// A view model whose file holds every animation: the frame it rests on and its firing frames.
struct HeldFrames {
  std::string_view model;
  int rest = 0;        // the first idle frame
  int fire_first = 0;  // the firing frames, both ends included
  int fire_last = 0;
};

// The sequel's view models: up 0..a, firing a+1..f, idle from f+1.
inline constexpr HeldFrames kHeldFrames[] = {
    {"models/weapons/v_blast/tris.md2", 9, 5, 8},
    {"models/weapons/v_shotg/tris.md2", 19, 8, 18},
    {"models/weapons/v_shotg2/tris.md2", 18, 7, 17},
    {"models/weapons/v_machn/tris.md2", 6, 4, 5},
    {"models/weapons/v_chain/tris.md2", 32, 5, 31},
    {"models/weapons/v_launch/tris.md2", 17, 6, 16},
    {"models/weapons/v_rocket/tris.md2", 13, 5, 12},
    {"models/weapons/v_rail/tris.md2", 19, 4, 18},
};

// The frames of the view model `model`, or null when its first frame is its rest and the rest its
// shot (the classic game's own).
constexpr const HeldFrames* held_frames(std::string_view model) {
  for (const HeldFrames& f : kHeldFrames)
    if (f.model == model) return &f;
  return nullptr;
}

}  // namespace shooters_edge::profiles
