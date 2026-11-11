// QUICK WEAPON, FOR ANY SHOOTER: a fire key that fires the best weapon carried by a priority list,
// then puts the usual one back. It runs on the client, out of what a HUD already knows (which
// weapons are carried and loaded, the one in hand and its ammunition), and asks the server only
// for what a player could: a weapon by its number and the attack. So it works in a game's own
// match and on any server joined. Weapons are the game's own numbers, 1..31 (its weapon keys).
//
// - TWO FIRE KEYS (`QuickKeys::fire[0]` and `[1]`, a game's +attack and +attack2), each with its
//   list (`QuickSettings::prio[k]`, best first) and on or off (`on[k]`; off, or an empty list,
//   the key fires whatever is in hand). The last one pressed owns the attack.
// - PRESSED: the first weapon of its list that is `ready` (carried, with a shot's ammunition) is
//   asked for in every command with the attack until it is in hand (so a server that drops an
//   ask a reloading weapon could not take switches, then fires, the moment it is free), and fired
//   for as long as the key is held (the next of the list when it runs dry). Let go: the first ready
//   weapon of `fallback` is asked for, the weapon kept in hand.
// - A TAP STILL FIRES: let go before the shot is seen (the weapon not yet in hand, or still
//   reloading from its last shot), the attack is held until the server shows the shot (the
//   weapon's ammunition fell below what it was at the press; a weapon that spends none after
//   `kMeleeHold` in hand), at most
//   `kLatchMax` from the press.
// - The way back is asked every `kResend` until the weapon is in hand, so one lost or refused does
//   not strand it; it gives up after `kReturnMax`. A weapon the player picks
//   himself (`QuickKeys::own_select`) wins: the way back is dropped. A dead player is let be.
// - THE LOOK (`quick_look`), for key 0: `preview` keeps the weapon it would fire in the hand the
//   whole time, a see-through ghost (`kGhost` of it drawn) while another is in hand: pressed, it
//   turns solid over `kSolidS` and fires, stays solid at least `kHoldS` more (a tap's shot is seen
//   on a solid weapon); let go, it fades back to the ghost over `kGhostS` while
//   the weapon kept in hand comes back unseen. So a tap never shows a weapon switch, only the ghost
//   turning solid and back. `hide` (the preview off) draws no weapon while the weapon in hand is
//   not the one it would fire. While key 1 owns the attack (and its way back) the hand shows the
//   weapon in hand as it is.
//
// Pure and allocation-free per frame (the lists are parsed when a setting changes). Held by
// tests/shooter_quick_weapon_test.cpp and the `se-quickweapon` lab.
//
// ge:summary Quick weapon for any shooter: a fire key fires the best weapon carried, then
// ge:summary switches back
// ge:keywords quick-weapon quickfire weapon-priority best-weapon switch-back attack2 fire-key
// ge:keywords cl_quickweapon cl_quickweapon_prio cl_quickweapon_prio2 cl_quickweapon_default
// ge:keywords cl_quickweapon_hide cl_quickweapon_preview ghost-weapon weapon-preview quick_step
// ge:skill shooter input
// ge:test tests/shooter_quick_weapon_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ge::shooter {

inline constexpr double kResend = 0.2;     // s between asks for the same weapon
inline constexpr double kLatchMax = 1.0;   // s a tap's attack is held at most, from the press
inline constexpr double kReturnMax = 1.5;  // s the way back is asked for at most
inline constexpr double kMeleeHold = 0.1;  // s a weapon that spends nothing is fired once in hand
inline constexpr float kGhost = 0.5f;      // the preview's share drawn
inline constexpr double kSolidS = 0.06;    // s the preview takes to turn solid once pressed
inline constexpr double kHoldS = 0.15;     // s it stays solid after that, however short the tap
inline constexpr double kGhostS = 0.25;    // s it takes to fade back to the ghost once let go
inline constexpr int kMaxWeapon = 31;

// What the client knows of the player's weapons (a game fills it from its status).
struct QuickArmed {
  uint32_t ready = 0;  // bit n: weapon n carried, with a shot's ammunition
  int in_hand = 0;     // the weapon in hand (0: none)
  // By weapon: the ammunition it fires from (falls when it fires); -1 a weapon that spends none.
  std::array<int, kMaxWeapon + 1> ammo{};
  bool alive = true;
};

struct QuickSettings {
  std::array<bool, 2> on{false, true};   // the fire keys that quick-fire
  std::array<std::vector<int>, 2> prio;  // each key's list, best first
  std::vector<int> fallback;             // the weapon kept in hand, best first
  bool hide = false;
  bool preview = true;
};

// This frame's keys.
struct QuickKeys {
  std::array<bool, 2> fire{false, false};
  int own_select = 0;  // a weapon the player picked himself this frame (0 none)
};

// What the command carries.
struct QuickCommand {
  bool attack = false;
  int select = 0;  // the weapon to ask for (0 none)
};

// What the quick fire is doing, frame to frame.
struct QuickState {
  int key = -1;  // the fire key firing (-1 none)
  int target = 0;
  double pressed_at = -1e9, sent_at = -1e9;
  int target_ammo = 0;  // the target's ammunition when it was chosen (the most since)
  bool seen = false;    // the target was in hand
  double seen_at = 0;
  bool returning = false;  // asking for the weapon kept in hand
  double return_at = 0;
  std::array<bool, 2> held{false, false};  // the keys the last frame
  int last_key = -1;                       // the fire key pressed last
  // The look: how solid the weapon shown is (0 the ghost .. 1 solid) at `look_at` s (-1 never).
  float solid = 0;
  double look_at = -1;
};

// The weapon numbers in `text` ("7 8 5 6 3 2 4 1": any run of digits is one, 1..kMaxWeapon kept,
// anything else separates).
std::vector<int> weapon_list(std::string_view text);
// The first of `list` that is ready (0: none).
int best_ready(const QuickArmed& a, const std::vector<int>& list);

// One frame at `t` s.
QuickCommand quick_step(QuickState& q, const QuickSettings& s, const QuickArmed& a,
                        const QuickKeys& keys, double t);

// How the weapon in hand looks this frame.
struct QuickLook {
  bool hide = false;   // draw no weapon
  bool ghost = false;  // draw `weapon` at `opacity` in place of the one in hand
  bool rest = false;   // ... and it is not the one in hand: drawn at rest, no shot of its own
  int weapon = 0;
  float opacity = 1;
};
// The look at `t` s, the fade stepped on from the last call's (call it once a drawn frame).
QuickLook quick_look(QuickState& q, const QuickSettings& s, const QuickArmed& a, double t);

}  // namespace ge::shooter
