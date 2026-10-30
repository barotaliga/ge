// THE PLAYER'S VIEW, FRAME BY FRAME: what the window (and the `se-client` lab, which draws the
// window's frame without a window) keeps between frames to show the listen server, and the two
// things it makes each frame. No GPU: a camera pose and a 2D overlay.
//
// - THE EYES (`frame_view`): engine/camera's first-person rig on the player (view.hpp's
//   subject): the view height from the mod's view offset, a step up the stairs smoothed, a
//   landing's dip by the fall speed, a forced view (a spawn, a teleport) cut to at once.
// - THE MARKERS (`draw_markers`), until the world pass draws the effects: every rocket and
//   grenade in flight a hot dot, every spike a small one, and where the mod's explosions went
//   off (Listen::explosions) a blast, a disc that grows and fades over `kBlastS`.
//
// ge:summary The player's view frame by frame: the first-person eyes and the missile markers
// ge:keywords present view-state frame_view first-person eyes landing-dip teleport markers
// ge:keywords rockets blasts draw_markers on_screen projection overlay
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <cstddef>
#include <vector>

#include "camera/first_person.hpp"
#include "render/view.hpp"
#include "shooters_edge/client/listen.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

inline constexpr double kBlastS = 0.5;  // a blast's marker lasts, s

struct Blast {
  ge::camera::Vec3 at;  // engine metres
  double t = 0;         // when it went off, s
};

struct ViewState {
  ge::camera::FirstPersonRow row;
  ge::camera::FirstPersonState eyes;
  std::vector<Missile> missiles;
  std::vector<Blast> blasts;
  std::size_t explosions_seen = 0;  // of Listen::explosions
  bool was_grounded = true;
  float fall_speed = 0;  // the last frame's vertical speed, units/s
};

// The eye after `frames` server frames this wall frame of `dt` s at time `t` s (the missiles and
// blasts brought up to date too). The view's eye, look, fov and near are set; nothing else.
ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt);

// A point (engine metres) on a w x h screen from `view`; false behind the eye.
bool on_screen(const ge::render::FrameView& view, ge::camera::Vec3 p, int w, int h, float& sx,
               float& sy);

// The rockets and blasts into `dl` (begun at w x h).
void draw_markers(ge::ui::DrawList& dl, const ViewState& v, const ge::render::FrameView& view,
                  int w, int h, double t);

}  // namespace shooters_edge::client
