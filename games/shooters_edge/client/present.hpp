// THE PLAYER'S VIEW, FRAME BY FRAME: what the window (and the `se-client` lab, which draws the
// window's frame without a window) keeps between frames to show the listen server. No GPU.
//
// - THE EYES (`frame_view`): engine/camera's first-person rig on the player (view.hpp's
//   subject): the view height from the mod's view offset, a step up the stairs smoothed, a
//   landing's dip by the fall speed, a forced view (a spawn, a teleport) cut to at once; a blow
//   the player took (Listen::blows) kicks the view away from where it came from, by its size
//   (armour and blood together, a full kick at `kFullBlow`) up to `kHitKickDeg`; the program's
//   punch (its recoil kick, decaying on the server) is added to the view's angles, never the aim.
// - THE MISSILES in flight and the BLASTS (where the mod's explosions went off,
//   Listen::explosions, kept for `kBlastS`): what the effects (world_fx.hpp) are thrown from.
//
// ge:summary The player's view frame by frame: the first-person eyes, the missiles and blasts
// ge:keywords present view-state frame_view first-person eyes landing-dip teleport missiles
// ge:keywords rockets blasts damage-kick hit-kick punchangle punch blows
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <cstddef>
#include <vector>

#include "camera/first_person.hpp"
#include "render/view.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

inline constexpr double kBlastS = 0.5;     // a blast is kept, s
inline constexpr double kFullBlow = 40;    // armour and blood that kick the view fully
inline constexpr double kHitKickDeg = 10;  // the full kick's pitch and roll, degrees

struct Blast {
  ge::camera::Vec3 at;  // engine metres
  double t = 0;         // when it went off, s
};

struct ViewState {
  ge::camera::FirstPersonRow row;
  ge::camera::FirstPersonState eyes;
  ge::camera::Pose pose;  // the last frame's eye
  std::vector<Missile> missiles;
  std::vector<Blast> blasts;
  std::size_t explosions_seen = 0;  // of Listen::explosions
  std::size_t blows_seen = 0;       // of Listen::blows
  bool was_grounded = true;
  float fall_speed = 0;  // the last frame's vertical speed, units/s
};

// The eye after `frames` server frames this wall frame of `dt` s at time `t` s (the missiles and
// blasts brought up to date too). The view's eye, look, fov and near are set; nothing else.
ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt);

}  // namespace shooters_edge::client
