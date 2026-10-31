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
//   The melee weapon is taller than it is long and is held upright by the bottom of its handle,
//   its edge along -z like a barrel.
// - THE MODELS: a weapon's model is its built asset (`content/models/view/<name>/`: a Blender
//   builder, `ge asset build <name>`, its texture baked into the .glb) or a free model dropped in
//   as `content/models/view/<name>.glb`; `weapon_model_path` picks. A model's base colour texture
//   is kept whole as the held skin, its emissive texture as the skin's glow. A built model holds
//   itself: its HANDS and arms are nodes named `<name>.hand...`, drawn with it but left out of the
//   box the mount measures. Such a model is drawn at kBuiltScale with its origin (the grip hand's
//   hold) at the look's offset; `length_m` is for a model dropped in.
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
// ge:keywords load_weapon_model weapon_model_path built-asset candidate axe minigun railgun
// ge:keywords lightning-gun grenade-launcher double-shotgun hands arms gloves
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
// The rate a model's clips are baked at into held frames (blended between when drawn).
inline constexpr double kClipFps = 30;

// THE BUILT WEAPONS' ARMS (content/models/view/weapon_kit.py, fed by the `se_view` data set): one
// body for every weapon. Each built weapon and the arms holding it are drawn at kBuiltScale of
// their true size, so the hands are the same size in every weapon; its origin is the grip hand's
// hold, put at the look's offset. The shoulders are where they are from the eye, in true metres
// (right, up, forward), the same for every weapon: they move only with the body (an axe's swing).
// They sit 6 cm higher and 10 cm further forward than a body's (0.24 below and 0.06 behind the
// eye): with the anatomy's own, the hands holding a shouldered gun fall below the bottom of the
// view. The shoulders are never seen, and every joint still keeps its human range.
inline constexpr double kBuiltScale = 0.5;
inline constexpr double kShoulderM[2][3] = {{0.19, -0.18, 0.04}, {-0.19, -0.18, 0.04}};  // r, l
inline constexpr double kUpperArmM = 0.32;  // shoulder to elbow
inline constexpr double kForearmM = 0.27;   // elbow to wrist

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

// Where `look`'s model is in `dir` (content/models/view): its built asset's model
// (`<dir>/<name>/<name>.glb`, made by `ge asset build <name>`) when that folder holds one, else a
// model dropped in as `<dir>/<name>.glb`.
std::string weapon_model_path(const std::string& dir, const WeaponLook& look);

// The model at `path` as `look`'s weapon: loaded with its textures kept (its picture becomes the
// skin), its clips baked at kClipFps (viewmodel::held_with_clips; the hand plays `fire` on a shot),
// mounted on its rest pose and with its muzzle found. False with the reason in `error`. The staging
// lab (se-weapon-stage) loads a candidate this way, so it sits exactly as the game would hold it.
// `weapon_only`, when given, is the same weapon without its hands, mounted the same way (a lab
// measures the weapon's size by it).
bool load_weapon_model(const std::string& path, const WeaponLook& look, vm::HeldModel& out,
                       std::string* error, vm::HeldModel* weapon_only = nullptr);

// Every weapon's model from `dir` (content/models/view), in `Weapon` order, mounted and with its
// muzzle found. False with the reason in `error` when one is missing or does not load.
bool load_weapon_models(const std::string& dir, std::vector<vm::HeldModel>& out,
                        std::string* error);

// The game's placement row, before a weapon's offset and the player's cvars.
vm::PlaceRow weapon_row();

}  // namespace shooters_edge::view
