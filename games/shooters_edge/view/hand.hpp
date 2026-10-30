// THE HAND: what Shooter's Edge's first-person weapon does from display frame to display frame,
// given what the game says happened (the weapon the player holds, a shot fired), on top of the
// engine's placement (viewmodel/place.hpp). Presentation only: it reads the simulation's word and
// feeds nothing back.
//
// - THE SWITCH: when the game's weapon changes, the weapon in the hand is put away (the row's
//   `switch_s`), swapped for the new one at the bottom, and raised again; asked for another on the
//   way, it goes on down and raises the last one asked for. A shot of the new weapon during the
//   switch (the simulation changed it at once) snaps the new weapon up: the shot is never fired
//   from a weapon out of the picture.
// - THE SHOT: the weapon's kick (weapons.hpp) handed to the placement, and its FLASH: a point
//   light at the muzzle (held_to_world: where the gun's own lens puts it in the world), fading over
//   the weapon's `flash_s`, for the world's lamps and the gun's own light (`light_held`).
// - THE PLAYER'S CVARS (`ViewSettings`, `add_view_cvars`), all the player's own (archived):
//     r_drawviewmodel   draw the weapon at all (0: the hand still switches and flashes, unseen)
//     r_viewmodel_fov   the weapon's own lens, deg vertical (0: the view's)
//     cl_gunx cl_guny cl_gunz
//                       the weapon moved from its place: right, up, forward, m
//     cl_gunsway        the share of the sway, the stride's swing and the breath (0 still, 1 the
//                       game's, 2 twice)
//
// ge:summary Shooter's Edge's first-person hand: the switch, the shot's kick and flash, the cvars
// ge:keywords shooters-edge viewmodel hand weapon-switch put-away raise shot kick flash-light
// ge:keywords muzzle-light r_drawviewmodel r_viewmodel_fov cl_gunx cl_guny cl_gunz cl_gunsway
// ge:keywords step_hand held_row add_view_cvars view-settings
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#pragma once

#include <span>

#include "camera/pose.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/light.hpp"

namespace shooters_edge::view {

struct ViewSettings {
  bool draw = true;             // r_drawviewmodel
  double fov_deg = 0;           // r_viewmodel_fov, vertical (0: the view's)
  double gun_m[3] = {0, 0, 0};  // cl_gunx, cl_guny, cl_gunz
  double sway = 1;              // cl_gunsway
};

// The player's view-model cvars, bound to `out`.
void add_view_cvars(ge::console::Cvars& cvars, ViewSettings& out);

// The row `look` is placed by under `settings`: `base` with the weapon's offset moved by the
// player's, the lens and the sway's share.
vm::PlaceRow held_row(const WeaponLook& look, const ViewSettings& settings,
                      const vm::PlaceRow& base = weapon_row());

struct Hand {
  Weapon held = Weapon::shotgun;    // in the hand now
  Weapon wanted = Weapon::shotgun;  // the game's
  bool fresh = true;
  vm::PlaceState place;
  double flash_left = 0;  // s of the last shot's light still to fade
};

struct HandInput {
  vm::PlaceInput place;  // the look and the stride (its `shot` and `lowered` are the hand's)
  Weapon weapon = Weapon::shotgun;  // the game's weapon
  bool fired = false;               // a shot of it this frame
};

struct HandFrame {
  Weapon weapon = Weapon::shotgun;  // the model to draw
  vm::Placement placement;
  bool drawn = true;       // r_drawviewmodel, and not all the way down
  bool switching = false;  // the hand is between weapons
  bool flashing = false;   // `flash` is lit
  vm::PointLight flash;    // the shot's light, world (its intensity faded)
};

// One display frame of `dt` s seen through `view`. `models` is load_weapon_models's set (the
// muzzle the flash stands at); `motion` the comfort scale.
HandFrame step_hand(Hand& hand, const HandInput& in, const ViewSettings& settings,
                    std::span<const vm::HeldModel> models, const ge::camera::Pose& view, double dt,
                    double motion = 1.0);

}  // namespace shooters_edge::view
