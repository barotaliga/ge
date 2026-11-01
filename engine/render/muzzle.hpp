// A WEAPON'S MUZZLE FIRE, AS SEEN (docs/rendering.md § Muzzle fire): the flame a shot throws out
// of the barrel, the smoke it leaves and the sparks, as pure functions of the shot, the muzzle's
// pose and the clock. It is the explosions' fire and smoke (render/explosion.hpp) shaped for a
// barrel and drawn by the same pass (render/explosions.hpp).
//
// - THE LOOK (`MuzzleLook`) is every knob of one weapon's fire: the flame's length and width
//   along the barrel, how long it lasts, how hot it is, how many prongs it has round the axis (a
//   shotgun's star, none for a jet) and how deep they cut, its billows; the SMOKE it leaves, a
//   whole `BlastLook` of its own (a puff that leaves the muzzle at `smoke_speed` and slows in the
//   air, its amount, size, colour and life); the sparks (how many, how fast, how hot, how wide
//   the cone) and the flash's light. `muzzle_knobs` names them, `set_muzzle_knob` sets one by
//   name (clamped), and a "smoke.<knob>" name sets the puff's own (render/explosion.hpp's
//   `blast_knobs`). The presets (`muzzle_looks`): "muzzle.shotgun", "muzzle.nailgun",
//   "muzzle.grenade" (a launcher's thump), "muzzle.rocket" (a launcher's jet); a game copies
//   one and turns its knobs for its own weapon.
// - THE FLAME FOLLOWS THE GUN. A shot (`Shot`) says when and with which look; where the muzzle
//   is comes each display frame from the weapon's pose (`MuzzlePose`, the barrel's tip, the
//   way it points and its up), so the flame rides the firing animation's recoil and the
//   player's turn. `flame_at` is the flame at a moment on a pose: a ball stretched along the
//   barrel (`stretch` its length over its width), its base at the muzzle, swelling to its
//   length in a third of its life while it burns out and cools.
// - THE SMOKE AND THE SPARKS STAY IN THE WORLD: `smoke_of` is the puff as a `Blast` born at the
//   muzzle as it was when the shot went off (drawn and aged by the explosions' code), and
//   `shot_births` the sparks flung down the barrel's line in a cone.
// - THE FLASH (`muzzle_flash_at`) is the flame's light for the local lights: its glowing area
//   (its length times its width) times its core's blackbody luminance times what burns times
//   `flash`, in the core's colour.
//
// Pure: the same shot, pose and moment give the same bits (ge::core::libm, ge::core::Rng).
//
// ge:summary A weapon's muzzle fire as seen: its knobs and presets, the flame on the muzzle, smoke
// ge:keywords render muzzle muzzle-flash muzzle-fire weapon gun firing shot shotgun nailgun
// ge:keywords grenade-launcher rocket-launcher flame smoke puff sparks knobs preset recoil
// ge:keywords muzzle_looks set_muzzle_knob flame_at smoke_of shot_births muzzle_flash_at
// ge:keywords tuned_muzzle scale_muzzle knob-list first-person-scale
// ge:skill render
// ge:test tests/render_muzzle_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "render/explosion.hpp"

namespace ge::render {

// Every knob of one weapon's fire. The defaults are a generic rifle's.
struct MuzzleLook {
  std::string_view id = "muzzle";
  // The flame.
  double length_m = 0.32;    // along the barrel, at its longest
  double width_m = 0.14;     // across it
  double life_s = 0.05;      // the flame's whole life
  double core_k = 2000;      // its heat as it leaves the barrel, K
  double cool_k = 1400;      // the heat it ends at, K
  double glow = 0.05;        // how bright the flame looks, a share of a blackbody's (thin gas)
  double flash = 0.45;       // its light on its surroundings, a share of a blackbody's
  uint32_t prongs = 0;       // petals round the barrel's axis (0: a round jet)
  double prong_depth = 0.5;  // how deep they cut, 0..1
  double billows = 0.5;      // the flame's billows, 0..1.5
  double fray = 0.45;        // its fraying, 0..1
  double billow_size = 1.4;  // the billows' size against the flame's width, 0.4..3
  // The smoke it leaves: a puff, a blast of its own (render/explosion.hpp's knobs).
  BlastLook smoke{.id = "muzzle.smoke",
                  .radius_m = 0.12,
                  .grow_s = 0.05,
                  .smoke_grow = 1.4,
                  .rise = 0.5,
                  .fire = 0,
                  .flash = 0,
                  .smoke = 0.7,
                  .smoke_in_s = 0.04,
                  .smoke_s = 1.2,
                  .smoke_albedo = 0.45,
                  .smoke_tint = {0.95f, 0.95f, 0.95f},
                  .billows = 0.6,
                  .fray = 0.4,
                  .billow_size = 1.3,
                  .roll = 0.6,
                  .sparks = 0,
                  .debris = 0,
                  .scorch_m = 0,
                  .light_range_m = 0};
  double smoke_speed = 3;  // how fast the puff leaves the muzzle, m/s (it slows in the air)
  // The sparks.
  uint32_t sparks = 4;
  double spark_speed = 20;    // the fastest, m/s
  double spark_life_s = 0.2;  // the mean, s
  double spark_k = 2200;      // their heat at birth, K
  double spread = 0.2;        // the cone's half angle, rad
  // The light.
  double light_range_m = 8;  // where the flash's light ends, m
};

// The engine's presets.
std::span<const MuzzleLook> muzzle_looks();
const MuzzleLook& default_muzzle();  // "muzzle.shotgun"
// The preset a content table names, or null.
const MuzzleLook* muzzle_look_by_id(std::string_view id);

// A knob of the flame or its sparks by name (the smoke's are blast_knobs as "smoke.<name>").
struct MuzzleKnob {
  std::string_view name, unit, what;
  double MuzzleLook::*number = nullptr;  // one of these two
  uint32_t MuzzleLook::*count = nullptr;
  double lo, hi;
};
std::span<const MuzzleKnob> muzzle_knobs();
// Sets the knob `name` ("smoke.<knob>" for the puff's) to `value`, clamped to its range:
// false for no such knob.
bool set_muzzle_knob(MuzzleLook& look, std::string_view name, double value);

// A preset turned by a knob list, "knob=value,knob=value" (set_muzzle_knob's names, "smoke.<knob>"
// the puff's), into `out`: false for a preset or a knob not known, or a value that is no number.
// A game's table of weapons names its fire this way.
bool tuned_muzzle(std::string_view preset, std::string_view knobs, MuzzleLook* out);

// Every size of `look` times `s`: the flame's length and width, the puff's radius and speed and the
// sparks' speed (the light's range is the room's, kept). A weapon drawn at a share of its true size
// near the eye (a first-person model, viewmodel/place.hpp's lens match) fires a flame of the same
// share.
void scale_muzzle(MuzzleLook& look, double s);

// Where the muzzle is this frame: the barrel's tip, the unit way it points, and the gun's up
// (unit, square to `dir`; it turns a flame's prongs with the gun's roll).
struct MuzzlePose {
  double at[3] = {0, 0, 0};
  double dir[3] = {0, 0, 1};
  double up[3] = {0, 1, 0};
};

// One shot fired.
struct Shot {
  const MuzzleLook* look = nullptr;  // null: the default; must outlive the shot
  uint32_t seed = 0;                 // its sparks, the turn of its prongs and its noise
  double born_s = 0;                 // the clock it was fired at
};
inline const MuzzleLook& look_of(const Shot& s) { return s.look ? *s.look : default_muzzle(); }

// The flame at a moment, on a pose.
struct Flame {
  double centre[3];  // world, m: the stretched ball's middle
  double axis[3];    // the barrel's way (unit)
  double side[3];    // square to it: where the first prong points (unit)
  double radius_m;   // across
  double stretch;    // its length over its width (1: a ball)
  double core_k;
  double fire;  // the share burning, 0..1
  double age_s;
  bool alive;
};
Flame flame_at(const Shot& s, const MuzzlePose& pose, double now_s);

// The puff the shot leaves, born at the muzzle's pose as it was when the shot went off.
Blast smoke_of(const Shot& s, const MuzzlePose& pose_at_shot);

// The flame's light, on the pose at that moment.
Flash muzzle_flash_at(const Shot& s, const MuzzlePose& pose, double now_s);

// Upper bound on what one shot of `look` throws.
size_t shot_births_max(const MuzzleLook& look);
// The sparks the shot throws from the pose it was fired on, written into `out` (at most its size):
// the count written.
size_t shot_births(const Shot& s, const MuzzlePose& pose_at_shot, std::span<Spark> out);

}  // namespace ge::render
