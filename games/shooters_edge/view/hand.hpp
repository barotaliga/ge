// THE HAND: what Shooter's Edge's first-person weapon does from display frame to display frame,
// given what the game says happened (the weapon the player holds, a shot fired), on top of the
// engine's placement (viewmodel/place.hpp). Presentation only: it reads the simulation's word and
// feeds nothing back.
//
// - THE SWITCH: when the game's weapon changes, the weapon in the hand is put away, swapped for
//   the new one at the bottom, and raised again, the whole in `cl_gunswitch` s (a fifth of a
//   second: `kSwitchDownShare` of it down, the rest up); asked for another on the way, it goes on
//   down and raises the last one asked for. A shot of the new weapon during the switch (the
//   simulation changed it at once) takes the old weapon out of the picture and RUSHES the new one
//   up from low in the picture (`kRushFrom`, or from where it is, already rising),
//   `cl_gunswitch_fire` times faster, firing as it comes: the shot is never held back by the
//   picture. 0 switches at once.
// - THE SHOT: the weapon's kick (weapons.hpp) handed to the placement, and its FLASH: a point
//   light at the muzzle (held_to_world: where the gun's own lens puts it in the world), fading over
//   the weapon's `flash_s`, for the world's lamps and the gun's own light (`light_held`).
// - THE ANIMATION: a shot plays the model's `fire` clip from its start (the pump racked, the
//   barrels turned, the axe's swing: viewmodel::held_clip_frames), handed out as the frames to
//   draw; a model without one draws its rest frame. A model with several `fire` clips (its TAKES:
//   a classic axe's two swings) plays one a shot: the take whose frames hold the frame the game
//   says the shot starts on (`HandInput::fire_frame`, a classic mod's weapon frame), or, when the
//   game does not say, one picked at random with even odds, as Quake picks a swing. A
//   clip stepped a frame a shot (`HeldClip::frame_a_shot`, a nailgun held firing) goes one frame
//   round on each shot and holds it, and the shot's muzzle, flash and fire are on the barrel that
//   frame fires from: the nailgun's two in turn, the super nailgun's cluster one barrel at a time.
//   With `ViewSettings::lerp` each shot's frame is drawn FROM THE LAST SHOT'S (or from rest on the
//   first) over the clip's frame time, so the barrels turn and the slide runs every display frame
//   instead of jumping ten times a second, then held there until the next shot; the model's own
//   flash is drawn at the shot's frame, never stretched between two frames (render/viewmodel.hpp).
//   Without it each shot's frame is shown at once and held. A clip played or held ends by flowing
//   back to the rest frame over a frame's time, never a jump.
// - DEATH (`HandInput::dead`): the weapon sinks out of view (`kDeathLowerS`), up with a new life.
// - A CLASSIC GAME'S OWN WEAPONS (`Hand::at_eye`): models authored at the eye, placed by the
//   classic row (`eye_row`) with no offset of their own; the kick, the flash and the fire are
//   still each weapon's look.
// - THE PLAYER'S CVARS (`ViewSettings`, `add_view_cvars`; each one's unit and range in its help),
//   all archived: r_drawviewmodel (0: still switches and flashes, unseen), r_viewmodel_fov,
//   cl_gunx/y/z, cl_gunsway (sway, stride and breath), cl_gunidle, cl_gunidle_rate,
//   cl_gunidle_exertion (place.hpp's THE BREATH), r_viewmodel_center (1, the default: centred under
//   the crosshair, nearer and higher by kCentreNearM, kCentreRiseM; 0 to the right),
//   r_viewmodel_hands (0: the weapon alone), cl_gunswitch and cl_gunswitch_fire (THE SWITCH).
// - THE MUZZLE each frame: where the barrel's tip is in the world (the firing animation's frame,
//   the kick, the lens match), the way it points and the gun's up, for the engine's muzzle fire
//   (render/muzzle.hpp) the window stages from it; `fired` says a shot left it this frame.
//
// ge:summary Shooter's Edge's first-person hand: the switch, the shot's kick and flash, the cvars
// ge:keywords shooters-edge viewmodel hand weapon-switch put-away raise shot kick flash-light
// ge:keywords muzzle-light r_drawviewmodel r_viewmodel_fov cl_gunx cl_guny cl_gunz cl_gunsway
// ge:keywords step_hand held_row add_view_cvars view-settings fire-clip animation frames death
// ge:keywords r_viewmodel_center centred-weapon centered r_viewmodel_hands hide-hands muzzle-pose
// ge:keywords at_eye eye_row classic-weapons axe-swing two-swings takes fire_frame weaponframe
// ge:keywords cl_gunidle cl_gunidle_rate cl_gunidle_exertion idle-sway breathing breath
// ge:keywords cl_gunswitch cl_gunswitch_fire switch-speed rushed-raise fire-during-switch
// ge:keywords r_viewmodelsize r_viewmodeloffset cl_gun_x cl_gun_y cl_gun_z cl_gunfov game_offset_m
// ge:keywords gun_next gun_prev gun_model add_gun_commands gun-frame-override
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "camera/pose.hpp"
#include "console/console.hpp"
#include "console/cvar.hpp"
#include "core/prng.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/light.hpp"

namespace shooters_edge::view {

struct ViewSettings {
  bool draw = true;             // r_drawviewmodel
  double fov_deg = 0;           // r_viewmodel_fov, vertical (0: the view's)
  double gun_m[3] = {0, 0, 0};  // cl_gunx, cl_guny, cl_gunz
  double sway = 1;              // cl_gunsway
  double idle = 1;              // cl_gunidle
  double idle_rate = 15;        // cl_gunidle_rate, breaths a minute at rest
  double idle_exertion = 1;     // cl_gunidle_exertion
  bool centre = true;           // r_viewmodel_center
  bool hands = true;            // r_viewmodel_hands
  int hand = 0;      // Quake II's `hand` (user info): 1 left, 2 in the middle (no weapon drawn)
  int gun = 1;       // Quake II's `cl_gun`: 0 or -1 none, 1 by `hand`, 2 always right, 3 left
  double alpha = 1;  // Quake II's `cl_gunalpha`: how solid the weapon is drawn, 0..1
  bool last_fired = false;  // r_viewmodellastfired (Quake's): a recording shows the last fired
  bool lerp = true;  // r_lerpmodels (bound with the picture's): a clip's end flows back to rest
  double switch_s = 0.2;   // cl_gunswitch, s a whole switch takes (0: at once)
  double switch_fire = 4;  // cl_gunswitch_fire, a shot's speed-up of the raise
  // The games' own spellings of the gun's place and lens, in their units (cvar_layers.cpp): each
  // adds to ours (`gun_m`) or stands in for ours unset (`fov_deg`).
  double size = 1;                  // r_viewmodelsize (Quake's): depth 0.5 + size / 2
  std::string offset_text;          // r_viewmodeloffset (Quake's): "right down forward"
  double gun_units[3] = {0, 0, 0};  // cl_gun_x, cl_gun_y, cl_gun_z (Quake II's): right, forward, up
  double gun_fov_h = 0;             // cl_gunfov (Quake II's): degrees across at 4:3 (0: none)
  double unit_m = 0.025;            // a game unit, m (the brush maps')
  // The developer's look at a weapon (`gun_next`, `gun_prev`, `gun_model`): a frame shown in place
  // of the one the game asks (0: the game's) and a weapon of the game's set shown in place of the
  // one held (-1: the game's). Neither is a setting: nothing saves them.
  int gun_frame = 0;
  int gun_model = -1;
};

// The side the weapon is drawn on: 0 not drawn, 1 right, -1 mirrored to the left. `cl_gun` 1
// follows `hand` (0 right, 1 left, 2 none); 2 and 3 force right and left whatever the hand.
int gun_side(const ViewSettings& s);

// The gun's offset the games' own settings add, m, right, up and forward: `r_viewmodeloffset`'s
// up to three numbers (missing ones 0, extra ones ignored; the second is DOWN) and `cl_gun_x/y/z`.
void game_offset_m(const ViewSettings& s, double out[3]);

// How far a model authored at the eye (`eye_row`) drops and tips when put away, m and rad: just
// out of the picture's bottom edge at the bottom of a switch, so every frame of a fast switch shows
// the weapon moving instead of most of it spent out of view.
inline constexpr double kEyeLowerM = 0.07;
inline constexpr double kEyeLowerPitch = 0.3;

// The share of a switch spent putting the old weapon away; the rest raises the new one.
inline constexpr double kSwitchDownShare = 0.4;

// How far put away a weapon rushed up by a shot during a switch starts (0 up, 1 out of view):
// low in the picture, but in it, so the frame the shot leaves shows the weapon it left from.
inline constexpr double kRushFrom = 0.7;

// How long the weapon takes to sink out of view when the player dies, s: slower than a switch's
// put-away, so the death reads as the hand letting go. It comes back up at the switch's pace.
inline constexpr double kDeathLowerS = 0.6;

// How much nearer and higher a centred weapon is held than the place to the right, m
// (r_viewmodel_center): seen down its length it would read as a thin stub, so it is brought in
// and lifted until its body shows; its muzzle still sits under the crosshair, not on it.
inline constexpr double kCentreRiseM = 0.01;
inline constexpr double kCentreNearM = 0.06;

// `gun_next` and `gun_prev` (the frame shown, one on or back, held at 0; each says "frame <n>") and
// `gun_model <name>` (the weapon of `held` named `models/<name>/tris.md2` shown in hand; no name:
// the game's again), over `out`. `held` must outlive the console (a game's static list).
void add_gun_commands(ge::console::Console& con, ViewSettings& out,
                      std::span<const std::string_view> held);

// The player's view-model cvars, bound to `out`.
void add_view_cvars(ge::console::Cvars& cvars, ViewSettings& out);

// The row `look` is placed by under `settings`: `base` with the weapon's offset moved by the
// player's, the lens and the sway's share.
vm::PlaceRow held_row(const WeaponLook& look, const ViewSettings& settings,
                      const vm::PlaceRow& base = weapon_row());

// The row a model authored at the eye is placed by under `settings`: the classic row with no
// offset but the player's (cl_gunx..z), the lens and the sway's share (r_viewmodel_center does
// not move it: such a model is where its game holds it).
vm::PlaceRow eye_row(const ViewSettings& settings);

struct Hand {
  Weapon held = Weapon::shotgun;    // in the hand now
  Weapon wanted = Weapon::shotgun;  // the game's
  bool fresh = true;
  vm::PlaceState place;
  double flash_left = 0;  // s of the last shot's light still to fade
  double fire_t = -1;     // s into the `fire` clip (-1: at rest)
  int shot_frame = -1;  // a clip stepped a frame a shot: the frame of the clip the last shot is on
  int shot_from = 0;    // ... and the model's frame it is drawn from while it turns (0: rest)
  int barrel = -1;      // the barrel the last shot left from (-1: the one muzzle)
  // Drawn between frames (ViewSettings::lerp): the frame a played or held clip ended on and the
  // time since, while it settles back to rest.
  int settle_from = 0;
  double settle_t = -1;
  int take = 0;  // the `fire` clip's take the last shot plays (a swing of two)
  ge::core::Rng takes = ge::core::make_rng(0x5a1e);  // picks a take when the game does not say
  // The models are a classic game's own (GameAdapter::held_weapons), authored at the eye: placed
  // by `eye_row`, never on a look's offset.
  bool at_eye = false;
  // A shot came during the switch: the new weapon is coming up `cl_gunswitch_fire` times faster
  // until it is all the way up.
  bool rushed = false;
};

struct HandInput {
  vm::PlaceInput place;  // the look and the stride (its `shot` and `lowered` are the hand's)
  Weapon weapon = Weapon::shotgun;  // the game's weapon
  bool fired = false;               // a shot of it this frame
  int fire_frame = 0;  // the model's frame the game says the shot starts on (0: not said)
  bool dead = false;   // the player is dead: the weapon sinks out of view (kDeathLowerS)
  bool q2 = false;     // the weapon is Quake II's (its looks: q2_weapon_look)
  // The game sends the gun's own sway (`place.game_turn`, `place.game_shift_m`): ours, the lag
  // behind a turn and the swing across, is set aside so it is not swayed twice.
  bool game_sway = false;
};

struct HandFrame {
  Weapon weapon = Weapon::shotgun;  // the model to draw
  vm::Placement placement;
  bool drawn = true;             // r_drawviewmodel, and not all the way down
  bool switching = false;        // the hand is between weapons
  bool rushed = false;           // ... and a shot is rushing the new one up
  bool flashing = false;         // `flash` is lit
  vm::PointLight flash;          // the shot's light, world (its intensity faded)
  int frame_a = 0, frame_b = 0;  // the model's frames to draw, blended by `frame_t`
  double frame_t = 0;
  double lowered = 0;                // how far put away, 0 up .. 1 out of view
  bool hands = true;                 // draw the hands holding it (r_viewmodel_hands)
  bool fired = false;                // a shot left the muzzle this frame
  double muzzle_at[3] = {0, 0, 0};   // the barrel's tip, world, m
  double muzzle_dir[3] = {0, 0, 1};  // the way it points, world, unit
  double muzzle_up[3] = {0, 1, 0};   // the gun's up, square to it
  // A gun with an arc across its prongs (HeldModel::arc_ends): its two ends, world, m.
  bool arc = false;
  double arc_from[3] = {0, 0, 0}, arc_to[3] = {0, 0, 0};
  float opacity = 1;  // the share drawn: below 1 a see-through ghost (the quick weapon's preview)
};

// One display frame of `dt` s seen through `view`. `models` is load_weapon_models's set (the
// muzzle the flash stands at); `motion` the comfort scale.
HandFrame step_hand(Hand& hand, const HandInput& in, const ViewSettings& settings,
                    std::span<const vm::HeldModel> models, const ge::camera::Pose& view, double dt,
                    double motion = 1.0);

}  // namespace shooters_edge::view
