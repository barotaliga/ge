// A GAME'S ARSENAL AS DATA: its weapons and its armour, one row each, which our programs
// (games/shooters_edge/rules/common_*.cpp) fire, show, pick up and absorb through instead of naming
// a weapon by a constant (docs/shooters-edge.md § Weapons and armour).
//
// - **A weapon row** (`WeaponRow`): its slot (the number key, 1..8, and the order the best-weapon
//   choice prefers); how it fires (`Fire`: a cut in front, pellets, a spike, a bouncing grenade,
//   a rocket, a beam); the ammunition it spends and how much a shot; how often it fires; its
//   pellets, the damage of each, their spread; its reach or its missile's speed; a missile's life
//   (a grenade's fuse); the splash (a blast's radius damage; for a beam, what a discharge in
//   water does per cell); and by the content's names its view model, its fire sound, its
//   missile's model, the sound its shot makes where it lands (a cut on a wall, a grenade's
//   bounce) and its blast's sound; the model it lies on a map as and the ammunition that comes
//   with it. "-" is none.
// - **An armour row** (`ArmourRow`): its slot (1..3: the map's three armour items, weakest
//   first), the share of a blow it takes, its amount, its pickup's model, skin and sound.
// - **Another game's looks** (`ItemLook`, classic remix): the model and pickup sound another game
//   lends the rules' ammunition and health (`set arsenal_ammo<n> "<model> <sound> <lift>"`,
//   `arsenal_health<n>`), and a weapon's pickup sound (`arsenal_pickup`).
// - **Into the program**: `arsenal_lines` writes each row as a server setting
//   (`set arsenal_weapon<slot> "<fields>"`, `set arsenal_armour<slot> "<fields>"`), which the
//   program reads with `cvar_string` (an extension both program forms have) as a map starts; a
//   slot with no row keeps the program's own. The field order is `weapon_text`'s and
//   `armour_text`'s, and the program's `arsenal_keep` and `armour_start` read the same order.
//
// ge:summary A game's weapons and armour as data rows our programs fire and absorb through
// ge:keywords arsenal weapon-row armour-row weapon-slot armour-slot fire-kind ammunition
// ge:keywords arsenal_lines weapon_text armour_text arsenal_weapon arsenal_armour kclassicarsenal
// ge:skill shooter progs
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <charconv>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::profiles {

// How a weapon fires: the program's own code for each, the row's numbers in it.
enum class Fire : uint8_t { melee, pellets, spike, grenade, rocket, beam };
// The ammunition a weapon spends.
enum class Ammo : uint8_t { none, shells, nails, rockets, cells };

struct WeaponRow {
  int32_t slot = 0;  // 1..8: its number key and its rank
  Fire fire = Fire::melee;
  Ammo ammo = Ammo::none;
  int32_t cost = 0;   // ammunition a shot
  float refire = 0;   // s between shots
  int32_t count = 1;  // pellets a shot
  float damage = 0;   // each pellet's, the cut's, the spike's, the rocket's direct hit, the beam's
  float spread = 0;   // the pellets' cone, a share of the forward line
  float reach = 0;    // a cut's or a beam's reach, a pellet's range or a missile's speed, u
  float life = 0;     // s a missile flies (a grenade's fuse)
  float splash = 0;   // a blast's radius damage; a beam's discharge in water, a cell
  std::string_view view;  // the content's names, "-" for none
  std::string_view fire_sound;
  std::string_view missile;
  std::string_view land_sound;
  std::string_view blast_sound;
  std::string_view pickup = "-";  // the model it lies on a map as ("-": it never lies there)
  int32_t pickup_ammo = 0;        // the ammunition that comes with it
};

struct ArmourRow {
  int32_t slot = 0;  // 1..3, weakest first
  float absorb = 0;  // the share of a blow it takes
  int32_t amount = 0;
  int32_t skin = 0;
  std::string_view model;
  std::string_view sound;
};

// Another game's look for one of the rules' own items (classic remix): the server setting that
// carries it (`arsenal_ammo1`..4: shells, nails, rockets, cells; `arsenal_health1`..3: the small,
// the plain and the mega health), the model, the pickup sound ("-": the item's own) and how far
// above the floor the model's origin stands, units (that game's items are measured from their
// middle).
struct ItemLook {
  std::string_view setting;
  std::string_view model;
  std::string_view sound = "-";
  int32_t lift = 0;
};

struct Arsenal {
  std::span<const WeaponRow> weapons;
  std::span<const ArmourRow> armour;
  std::span<const ItemLook> looks = {};  // none: every other item its own
  std::string_view pickup_sound = "";    // a weapon's pickup sound ("": the rules' own)
};

// The classic game's eight weapons and three armours.
inline constexpr WeaponRow kClassicWeapons[] = {
    {1, Fire::melee, Ammo::none, 0, 0.5f, 1, 20, 0, 64, 0, 0, "progs/v_axe.mdl", "weapons/ax1.wav",
     "-", "player/axhit2.wav", "-", "-", 0},
    {2, Fire::pellets, Ammo::shells, 1, 0.5f, 6, 4, 0.04f, 2048, 0, 0, "progs/v_shot.mdl",
     "weapons/guncock.wav", "-", "-", "-", "-", 0},
    {3, Fire::pellets, Ammo::shells, 2, 0.7f, 14, 4, 0.07f, 2048, 0, 0, "progs/v_shot2.mdl",
     "weapons/shotgn2.wav", "-", "-", "-", "progs/g_shot.mdl", 5},
    {4, Fire::spike, Ammo::nails, 1, 0.1f, 1, 9, 0, 1000, 6, 0, "progs/v_nail.mdl",
     "weapons/rocket1i.wav", "progs/spike.mdl", "-", "-", "progs/g_nail.mdl", 30},
    {5, Fire::spike, Ammo::nails, 2, 0.1f, 1, 18, 0, 1000, 6, 0, "progs/v_nail2.mdl",
     "weapons/spike2.wav", "progs/s_spike.mdl", "-", "-", "progs/g_nail2.mdl", 30},
    {6, Fire::grenade, Ammo::rockets, 1, 0.6f, 1, 0, 0, 600, 2.5f, 120, "progs/v_rock.mdl",
     "weapons/grenade.wav", "progs/grenade.mdl", "weapons/bounce.wav", "weapons/r_exp3.wav",
     "progs/g_rock.mdl", 5},
    {7, Fire::rocket, Ammo::rockets, 1, 0.8f, 1, 100, 0, 1000, 5, 120, "progs/v_rock2.mdl",
     "weapons/sgun1.wav", "progs/missile.mdl", "-", "weapons/r_exp3.wav", "progs/g_rock2.mdl", 5},
    {8, Fire::beam, Ammo::cells, 1, 0.1f, 1, 30, 0, 600, 0, 35, "progs/v_light.mdl",
     "weapons/lhit.wav", "-", "-", "-", "progs/g_light.mdl", 15},
};
inline constexpr ArmourRow kClassicArmour[] = {
    {1, 0.3f, 100, 0, "progs/armor.mdl", "items/armor1.wav"},
    {2, 0.6f, 150, 1, "progs/armor.mdl", "items/armor1.wav"},
    {3, 0.8f, 200, 2, "progs/armor.mdl", "items/armor1.wav"},
};
inline constexpr Arsenal kClassicArsenal{kClassicWeapons, kClassicArmour};

inline constexpr std::string_view kFireWords[] = {"melee",   "pellets", "spike",
                                                  "grenade", "rocket",  "beam"};
inline constexpr std::string_view kAmmoWords[] = {"none", "shells", "nails", "rockets", "cells"};

// A number as the shortest text that reads back as the same float ("0.04", "2.5", "120").
inline std::string number_text(float v) {
  char buf[32];
  const auto r = std::to_chars(buf, buf + sizeof buf, v);
  return std::string(buf, r.ptr);
}

// A weapon row as the program reads it: the fields in order, a space between.
inline std::string weapon_text(const WeaponRow& w) {
  std::string t;
  for (const std::string& f :
       {std::string(kFireWords[static_cast<int>(w.fire)]),
        std::string(kAmmoWords[static_cast<int>(w.ammo)]), number_text(float(w.cost)),
        number_text(w.refire), number_text(float(w.count)), number_text(w.damage),
        number_text(w.spread), number_text(w.reach), number_text(w.life), number_text(w.splash),
        std::string(w.view), std::string(w.fire_sound), std::string(w.missile),
        std::string(w.land_sound), std::string(w.blast_sound), std::string(w.pickup),
        number_text(float(w.pickup_ammo))}) {
    if (!t.empty()) t += ' ';
    t += f;
  }
  return t;
}

// An armour row as the program reads it.
inline std::string armour_text(const ArmourRow& a) {
  return number_text(a.absorb) + " " + number_text(float(a.amount)) + " " +
         number_text(float(a.skin)) + " " + std::string(a.model) + " " + std::string(a.sound);
}

// The server's console lines that give the program `arsenal`.
inline std::vector<std::string> arsenal_lines(const Arsenal& arsenal) {
  std::vector<std::string> lines;
  for (const WeaponRow& w : arsenal.weapons)
    lines.push_back("set arsenal_weapon" + std::to_string(w.slot) + " \"" + weapon_text(w) + "\"");
  for (const ArmourRow& a : arsenal.armour)
    lines.push_back("set arsenal_armour" + std::to_string(a.slot) + " \"" + armour_text(a) + "\"");
  for (const ItemLook& l : arsenal.looks)
    lines.push_back("set " + std::string(l.setting) + " \"" + std::string(l.model) + " " +
                    std::string(l.sound) + " " + std::to_string(l.lift) + "\"");
  if (!arsenal.pickup_sound.empty())
    lines.push_back("set arsenal_pickup \"" + std::string(arsenal.pickup_sound) + "\"");
  return lines;
}

}  // namespace shooters_edge::profiles
