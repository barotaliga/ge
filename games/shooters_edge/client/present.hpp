// THE PLAYER'S VIEW, FRAME BY FRAME: what the window (and the `se-client` lab, which draws the
// window's frame without a window) keeps between frames to show the listen server. No GPU.
//
// - THE EYES (`frame_view`): engine/camera's classic first-person rig (`classic_first_person`) on
//   the player (view.hpp's subject), its numbers the player's camera cvars (`cl_bob`,
//   `cl_rollangle`, `v_kickpitch`... at the classic defaults, camera/cvars.hpp, `add_view_cvars`
//   below): the view height from the mod's view offset (16 units under the origin dead, the
//   view rolled 80 degrees), a step up the stairs smoothed, the stride's bob, the strafe's roll, a
//   forced view (a spawn, a teleport) cut to at once; a blow the player took (Listen::blows)
//   tips the view toward where it came from by its count (half the armour and blood it took, at
//   least `kKickCountMin`) times `v_kickroll` and `v_kickpitch`, fading over `v_kicktime`; the
//   program's punch (its recoil kick, decaying on the server) is added to the view's angles,
//   never the aim. `docs/camera.md` § Shooter's Edge's view. When the rules work the view out
//   themselves (the sequel's, `PlayerState::rules_view`), their eye offset and kick angles are
//   drawn between their last two frames and the rig adds no bob, roll or kick of its own; a step
//   up is eased at `kRulesStepRate` within `kRulesStepMost`; dead, the view is the rules'.
//   With `ViewState::lerp` (`r_lerpmove`) the eye's place is drawn one server frame behind,
//   between the last two states (as the models are, model_lerp.hpp), so moving never shudders
//   when the server's 72 or 77 frames a second fall unevenly on the display's; the look is the
//   mouse's every frame, never behind. A forced view or a move over `kEyeSnapUnits` cuts.
// - THE MISSILES in flight and the BLASTS (where the mod's explosions went off,
//   Listen::explosions, kept for `kBlastS`): what the effects (world_fx.hpp) are thrown from.
//
// ge:summary The player's view frame by frame: the first-person eyes, the missiles and blasts
// ge:keywords present view-state frame_view first-person eyes landing-dip teleport missiles
// ge:keywords rockets blasts damage-kick hit-kick punchangle punch blows eye-lerp smooth-eye
// ge:keywords add_view_camera_cvars classic-view view-bob strafe-roll dead-roll v_kickpitch
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <cstddef>
#include <vector>

#include "brush/map.hpp"
#include "camera/cvars.hpp"
#include "camera/first_person.hpp"
#include "render/view.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

inline constexpr double kBlastS = 0.5;         // a blast is kept, s
inline constexpr double kRulesStepRate = 180;  // units/s: a step's 18 units over a tenth (sequel)
inline constexpr double kRulesStepMost = 20;   // the most it lags, units (sequel)
inline constexpr double kKickCountMin = 10;    // the least count a blow kicks the view by
inline constexpr double kDeadRollDeg = 80;     // the view's roll dead
inline constexpr float kEyeSnapUnits = 64;     // the eye moving farther in one state: not between

struct Blast {
  ge::camera::Vec3 at;  // engine metres
  double t = 0;         // when it went off, s
  sh::Vec3f map_at;     // map units (the effects' sprite stands there)
};

struct ViewState {
  // The rig's row: the classic one with the player's cvars over it (`apply_view_cvars`).
  ge::camera::FirstPersonRow row = ge::camera::classic_first_person(ge::brush::kUnitM);
  double viewheight_m = 0;  // cl_viewheight over the origin; 0: the mod's view offset
  ge::camera::FirstPersonState eyes;
  ge::camera::Pose pose;  // the last frame's eye
  std::vector<Missile> missiles;
  std::vector<Blast> blasts;
  std::size_t explosions_seen = 0;  // of Listen::explosions
  std::size_t blows_seen = 0;       // of Listen::blows
  bool was_grounded = true;
  float fall_speed = 0;  // the last frame's vertical speed, units/s
  // The eye drawn between the server's states (r_lerpmove; off: each state as run): the last two
  // states' places of the player and their server times.
  bool lerp = false;
  sh::Vec3f eye_prev, eye_now;
  double eye_prev_s = -1, eye_now_s = -1;
  // The rules' own view (PlayerState::rules_view) at their last two frames and as drawn between.
  sh::Vec3f offset_prev, offset_now, kick_prev, kick_now, offset, kick;
};

// The camera's cvars a player turns (cl_bob, cl_bobcycle, cl_bobup, cl_rollangle, cl_rollspeed,
// v_kicktime, v_kickpitch, v_kickroll, v_idlescale, cl_fov, cl_viewheight...) at the classic
// look's defaults in map units, bound to `out`.
void add_view_camera_cvars(ge::console::Cvars& c, ge::camera::CameraCvars& out);
// The cvars written over the classic row into `v` (once a frame, before `frame_view`).
void apply_view_cvars(const ge::camera::CameraCvars& c, ViewState& v);

// The eye after `frames` server frames this wall frame of `dt` s at time `t` s (the missiles and
// blasts brought up to date too). The view's eye, look, fov and near are set; nothing else.
ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt);

}  // namespace shooters_edge::client
