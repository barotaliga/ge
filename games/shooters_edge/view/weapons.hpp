// SHOOTER'S EDGE'S HELD WEAPONS: the game's own first-person weapons, the free models in
// `content/models/view` (content/models.tsv, CREDITS.md), put into the engine's view-model layer
// (engine/viewmodel) without the classic pack: each one's form, its MOUNT (how it sits in the
// hand), its kick, how often it fires and the light its shot throws.
//
// - THE SET (`Weapon`) is the game's eight held weapons in the order of their number keys (impulse
//   1 to 8): the melee weapon, the shotgun, the double shotgun, the nailgun, the heavy nailgun, the
//   grenade launcher, the rocket launcher and the beam gun.
// - THE MOUNT (`WeaponLook`): the kit's guns point along -z once in the engine's frame and are
//   modelled at toy scale, so each is turned muzzle forward and scaled to its own held LENGTH (m),
//   gripped low and a third of the way from the back, and sat at its own OFFSET from the eye
//   (right, up, forward, m): a long launcher further out and lower than a short shotgun, so every
//   weapon fills about the same share of the picture's lower right and none is cut by its edge.
//   The melee weapon is taller than it is long and is held upright by the bottom of its handle.
// - THE MUZZLE is found on the mounted model (`viewmodel::find_muzzle`): the front-most vertices
//   along the barrel.
// - THE SHOT: its KICK (the size handed to the placement: back and up), its REFIRE (s between
//   shots while attack is held) and its FLASH: a point light at the muzzle (colour, intensity at
//   1 m in the held unit, range, how long it lasts), none for the melee weapon.
// - THE ROW (`weapon_row`): the engine's modern row (a stride's sway, weight on a turn, a breath
//   at rest) with the game's quicker put-away; the hand (hand.hpp) puts each weapon's offset and
//   the player's cvars on top of it.
//
// ge:summary Shooter's Edge's held weapons: the free set, each one's mount, kick, refire and flash
// ge:keywords shooters-edge viewmodel view-model held-weapon first-person weapon gun mount
// ge:keywords free-models glb kick refire muzzle-flash flash-light weapon-row weapon_looks
// ge:keywords load_weapon_models melee shotgun nailgun rocket-launcher beam-gun impulse
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "viewmodel/held.hpp"
#include "viewmodel/place.hpp"

namespace shooters_edge::view {

namespace vm = ge::viewmodel;

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
inline constexpr int kWeapons = 8;

struct WeaponLook {
  std::string_view name;  // the model's file stem under content/models/view
  double length_m;        // held length along the barrel (the melee weapon: its height)
  double offset_m[3];     // the grip from the eye: right, up, forward
  double kick;            // a shot's size for the placement's kick
  double refire_s;        // between shots while attack is held
  double flash_s;         // how long the shot's light lasts, fading (0: none)
  float flash_rgb[3];     // the shot's light: colour (linear)
  float flash_intensity;  // at 1 m, held unit
  float flash_range_m;
  Weapon weapon;
  bool upright;  // held upright by its handle (a melee weapon), not barrel forward
};

std::span<const WeaponLook> weapon_looks();
const WeaponLook& weapon_look(Weapon w);
// The weapon number `impulse` (1..8) selects, or false when it names none.
bool weapon_of_impulse(int impulse, Weapon& out);

// The mount `look` gives a model of these bounds (held_bounds before mounting).
vm::Mount weapon_mount(const WeaponLook& look, const float lo[3], const float hi[3]);

// Every weapon's model from `dir` (content/models/view), in `Weapon` order, mounted and with its
// muzzle found. False with the reason in `error` when one is missing or does not load.
bool load_weapon_models(const std::string& dir, std::vector<vm::HeldModel>& out,
                        std::string* error);

// The game's placement row, before a weapon's offset and the player's cvars.
vm::PlaceRow weapon_row();

}  // namespace shooters_edge::view
