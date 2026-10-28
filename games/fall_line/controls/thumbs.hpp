// FALL LINE'S THUMBS: a phone's screen as the skier's controls (engine/input/touch.hpp), the
// layout the default bindings name (bindings.hpp's `touch:` lines) and the HUD draws (M10).
//
//   THE EDGE THUMB  the left half: a floating stick under the thumb wherever it lands, read
//                   across as the steer (shaped `curve=1.15`, so a slight edge is a target and
//                   not the twitch either side of flat). Its centre trails a thumb swept past the
//                   reach, so the way back to the other edge is never longer than the throw.
//   THE TUCK LEVER  the right half: anchored in a FULL TUCK under the thumb (a thumb on the glass
//                   is a skier folded out of the wind, which a race asks of him nearly all the
//                   time); sliding down stands him up over one reach, and past a small dead band
//                   under that the same throw skids the skis across (`touch:lever+` the tuck,
//                   `touch:lever-` the brake), so one thumb never asks for both.
//   THE JUMP        on the lever's thumb, two ways a tuck never makes by accident: a FLICK up
//                   (sprung at once, as high as a key held longer the faster the flick), and a
//                   TAP then the thumb put back down and HELD (loaded while held, sprung when it
//                   lifts, as the key is).
//   RESET, CAMERA   two buttons in the top corners.
//
// The reaches are shares of the screen's height, so the controls are the same size under the
// thumb on any phone: `kEdgeReach` is about 90 px of a phone held on its side, `kLeverReach` 60.
//
// ge:summary Fall Line's touch layout: the edge thumb, the tuck lever, the jump's gestures
// ge:keywords fall-line touch thumbs touch-layout edge-thumb tuck-lever flick tap-hold phone
// ge:keywords mobile reset camera
// ge:skill input
// ge:test tests/fall_line_controls_test.cpp
#pragma once

#include "input/touch.hpp"

namespace fall_line::controls {

inline constexpr float kEdgeReach = 0.23f;   // the edge thumb's full throw, of the height
inline constexpr float kLeverReach = 0.15f;  // the lever's tuck to standing, of the height
inline constexpr float kLeverDead = 0.2f;    // the band before the skid, of the lever's reach
inline constexpr float kTopBand = 0.15f;     // the top of the screen left to the HUD's buttons

// The layout for a screen of `aspect` (its width over its height).
ge::input::TouchLayout touch_layout(float aspect = 16.0f / 9.0f);

}  // namespace fall_line::controls
