// WHAT THE HUD SHOWS OF A PLAYER: Shooter's Edge's status snapshot, written by the client once a
// frame from what the server last told it, and read by the HUD (layout.hpp) and nothing else.
// The HUD never reads the game: a snapshot in, pixels out, so a lab, a test or a demo draws it
// with no server running.
//
// The weapons are the arena's eight, the melee weapon always held; four kinds of ammunition feed
// the seven guns. Armour comes in three classes (light, medium, heavy) that soak a growing share
// of a hit. Four powerups run on a clock: the CLOAK (nearly invisible), the SHIELD (no damage
// taken), the SUIT (breathe under water, no harm from slime) and the DAMAGE boost (four times the
// damage dealt). Two keys open doors; four runes are carried between levels.
//
// Times are the client's clock in seconds (`t`): when each weapon was picked up (its icon flashes
// for a second after) and when the player was last hurt (the portrait winces).
//
// THE ITEMS WORD (`items`, `in_hand`): the 32 item bits and the weapon in hand as the program
// keeps them, for what the eight weapons above cannot say. A Quake mission pack's own status bar
// reads its new weapons and items off them (art.hpp's `HudArt::pack`, layout.cpp), and every
// gun's pickup flash is timed off a bit's rise (layout.hpp's `Hud::rose_at`). A program that
// defines `items2` sends it above bit 23 in place of the runes (Scourge of Armagon's wetsuit,
// Dissolution of Eternity's armour, ammunition and powerups).
//
// ge:summary Shooter's Edge's HUD snapshot: health, armour, ammunition, weapons, keys, powerups
// ge:keywords hud status snapshot health armour armor ammo shells nails rockets cells weapons
// ge:keywords weapon-group group-show
// ge:keywords keys powerups cloak shield suit damage runes frags scoreboard player-status
// ge:keywords items-word items2 in_hand mission-pack team-border current-ammo
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ui/paint.hpp"

namespace shooters_edge::hud {

enum class Weapon : uint8_t {
  melee,
  shotgun,
  double_shotgun,
  nailgun,
  heavy_nailgun,
  grenade_launcher,
  rocket_launcher,
  beam_gun,
};
inline constexpr int kGuns = 7;  // every weapon but the melee one: the HUD's row

enum class Ammo : uint8_t { shells, nails, rockets, cells, none };
inline constexpr int kAmmoKinds = 4;

enum class ArmourClass : uint8_t { none, light, medium, heavy };

enum class Power : uint8_t { cloak, shield, suit, damage };
inline constexpr int kPowers = 4;

// The ammunition a weapon fires (none for the melee weapon).
Ammo ammo_of(Weapon w);

// The weapon group shown (client/weapon_groups.hpp, with cl_weapongroups 1): open, the group's
// weapons carried, best first, the one asked for lit; folding (`open` falling to 0) into it;
// folded, the weapon in hand alone, `more` marking that its group holds another carried.
inline constexpr int kGroupShown = 4;
struct GroupShow {
  bool on = false;
  int count = 0;
  std::array<Weapon, kGroupShown> weapons{};
  std::array<bool, kGroupShown> ready{};  // with a shot's ammunition
  int lit = -1;
  float open = 0;  // 1 open .. 0 folded
  bool more = false;
};

// A row of the frag board the HUD shows beside its own figures.
struct FragRow {
  int frags = 0;
  ge::ui::Color top{1, 1, 1, 1}, bottom{1, 1, 1, 1};  // the player's two colours
  bool me = false;
};

// The weapons by the game's own number keys, for a game whose keys are not the eight `Weapon`s
// (Quake II's eleven): what the quick weapon and the weapon groups read. Bit k of `carried` and
// `ready` (carried, with a shot's ammunition) is the weapon on key k; `ammo` by key, -1 for one
// that spends none. Unset, they read the eight.
struct KeyedWeapons {
  bool set = false;
  uint32_t carried = 0, ready = 0;
  int in_hand = 0;  // its key; 0 none
  std::array<int, 32> ammo{};
};

struct Status {
  // In the order that packs it (the lint holds the padding).
  double t = 0;  // s, the client's clock
  double hurt_at = -1e9;
  std::vector<FragRow> board;             // sorted best first; the HUD shows at most four
  std::array<double, kPowers> power_s{};  // s left of each powerup; 0: not held
  std::array<double, kGuns> picked_at{-1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9};
  int health = 100;
  int armour = 0;
  int frags = 0;
  // The items word and the weapon in hand as the program's bits (above); 0: not known, the HUD
  // reads the other fields alone.
  uint32_t items = 0, in_hand = 0;
  int current_ammo = -1;  // the count beside the ammunition icon; -1: the weapon in hand's
  // Team games (Dissolution of Eternity's team border): the server's `teamplay`, whether it seats
  // more than one player (`multiplayer`), and the player's own shirt and pants colours (0..13; -1
  // not known).
  int teamplay = 0;
  int shirt = -1, pants = -1;
  std::array<int, kAmmoKinds> ammo{};
  GroupShow group;
  KeyedWeapons keyed;
  ArmourClass armour_class = ArmourClass::none;
  Weapon weapon = Weapon::shotgun;
  uint8_t owned = 0;  // bit (k - 1) for gun k: Weapon::shotgun is bit 0
  bool key_silver = false, key_gold = false;
  uint8_t runes = 0;  // bit k: rune k + 1
  bool dead = false;
  bool multiplayer = false;

  bool has(Weapon w) const {
    return w == Weapon::melee || (owned >> (static_cast<int>(w) - 1) & 1) != 0;
  }
  bool powered(Power p) const { return power_s[static_cast<size_t>(p)] > 0; }
  int ammo_for(Weapon w) const {
    const Ammo a = ammo_of(w);
    return a == Ammo::none ? 0 : ammo[static_cast<size_t>(a)];
  }
};

}  // namespace shooters_edge::hud
