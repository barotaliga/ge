// THE GAME PLAYED BY TOUCH (docs/hub.md § Playing in a browser): a phone's or a tablet's screen as
// the first game's controls, on engine/input's touch controls (input/touch.hpp). The left of the
// screen is a floating stick that walks (its centre where the thumb lands, trailing a thumb that
// pushes past it); the right is a drag that looks; at the right edge a fire button (held while
// touched), a jump button above it, and a weapon button in the top corner that takes the next
// weapon. A button's zone is tried before the look's, so a thumb on the fire button fires and
// looks no further.
//
// The stick reads as the walk keys (a push past a third of its travel in a direction holds that
// way's key), the drag as the mouse's look: `kTouchLookDegrees` across the screen's height.
//
// ge:summary The game played by touch: walk stick, look drag, fire, jump and weapon buttons
// ge:keywords touch touchscreen phone tablet mobile play-in-browser stick look-drag fire-button
// ge:keywords touch_play touch_moves draw_touch
// ge:skill input shooter
// ge:test tests/shooters_edge_touch_play_test.cpp
#pragma once

#include "input/state.hpp"
#include "input/touch.hpp"
#include "shooters_edge/client/controls.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

inline constexpr float kTouchLookDegrees = 180;  // a drag across the screen's height turns this
inline constexpr float kTouchPush = 0.33f;       // a stick's push that holds a walk key
inline constexpr int kNextWeaponImpulse = 10;    // the weapon button: the next weapon

struct TouchPlay {
  ge::input::TouchLayout layout;
  ge::input::TouchFrame frame;
  bool seen = false;  // a finger has touched: the controls are drawn
};

// The layout for a screen of `aspect` (width over height).
TouchPlay touch_play(float aspect);
// This frame's touch controls from `in`'s fingers into `m` (held keys added, never taken away) and
// the look in degrees (`yaw` + to the right, `pitch` + down). `now_s` is any rising clock.
void touch_moves(TouchPlay& t, const ge::input::InputState& in, double now_s, Moves& m, float& yaw,
                 float& pitch);
// The controls drawn over a `w` x `h` picture once a finger has touched: the stick where the
// thumb holds it, the buttons, faint until pressed.
void draw_touch(const TouchPlay& t, ge::ui::DrawList& d, int w, int h);

}  // namespace shooters_edge::client
