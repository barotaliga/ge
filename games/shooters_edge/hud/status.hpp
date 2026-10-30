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
// ge:summary Shooter's Edge's HUD snapshot: health, armour, ammunition, weapons, keys, powerups
// ge:keywords hud status snapshot health armour armor ammo shells nails rockets cells weapons
// ge:keywords keys powerups cloak shield suit damage runes frags scoreboard player-status
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

// A row of the frag board the HUD shows beside its own figures.
struct FragRow {
  int frags = 0;
  ge::ui::Color top{1, 1, 1, 1}, bottom{1, 1, 1, 1};  // the player's two colours
  bool me = false;
};

struct Status {
  double t = 0;  // s, the client's clock
  int health = 100;
  int armour = 0;
  ArmourClass armour_class = ArmourClass::none;
  std::array<int, kAmmoKinds> ammo{};
  Weapon weapon = Weapon::shotgun;
  uint8_t owned = 0;  // bit (k - 1) for gun k: Weapon::shotgun is bit 0
  std::array<double, kGuns> picked_at{-1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9};
  bool key_silver = false, key_gold = false;
  std::array<double, kPowers> power_s{};  // s left of each powerup; 0: not held
  uint8_t runes = 0;                      // bit k: rune k + 1
  double hurt_at = -1e9;
  int frags = 0;
  std::vector<FragRow> board;  // sorted best first; the HUD shows at most four
  bool dead = false;

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
