// THE TOUCH CONTROLS: a phone's or a tablet's screen read as controls a binding can name
// (`touch:<name>`). A layout is a list of controls, each a ZONE of the screen (0..1 from the top
// left); a finger belongs to the first control whose zone it touched down in, and keeps it
// until it lifts, wherever it slides.
//
//   button  held while its finger is down
//   stick   a floating stick: its centre is where the finger touched down; the push is the
//           finger's offset over `radius` (a share of the screen's height, so it is round on
//           any screen), at most 1. A `fixed` stick's centre is its zone's centre instead. A
//           `trail` stick's centre is dragged behind a finger that passes its radius, so the way
//           back is never longer than the push (a thumb that overshoots loses nothing).
//   lever   one value 0..1: `start` where the finger touched down, rising as it slides up and
//           falling as it slides down, one full travel over `radius`. A lever anchored full
//           (`start = 1`) is a throttle held by default that a thumb eases off. A `floor` below 0
//           gives it a second range under the first: past a `dead` band (a share of `radius`)
//           at 0 the value goes negative, down to `floor`, at the same travel (a brake under a
//           throttle on one thumb, read as the lever's two halves, `touch:<name>+` and `-`).
//   drag    the finger's motion this frame (a share of the height), for a look or a camera
//
// GESTURES are controls that claim no finger: they watch another control (`of`) and read as a
// button.
//
//   flick     the watched finger thrown UP fast: a rise of `rise` (a share of the height) or more
//             inside `window_s` at `speed` (heights a second) or faster presses it, held for
//             `hold_min_s` up to `hold_max_s` as the flick's speed reaches `full`; fired, it waits
//             for the finger to come back down `rearm` (or a new touch) before it fires again
//   tap_hold  a TAP (a touch lifted within `tap_s`), then the finger put back down within
//             `gap_s` and HELD: held while that second touch is (a jump loaded, sprung on release)
//
// A gesture reads the clock it is handed (`touch_update`'s `now_s`); without one it never fires.
//
// The frame is computed once (`touch_update`) after the input state's update and before the
// actions are evaluated, which read it through `Sources::touch`.
//
// ge:summary Touch controls: zones, sticks, buttons, levers, drags, flick and tap-hold gestures
// ge:keywords input touch touchscreen mobile phone tablet virtual-stick floating-stick thumb
// ge:keywords lever slider drag look zone touch-layout touch-frame touch_update button
// ge:keywords trail trailing-stick brake-range gesture flick tap-hold tap_hold double-tap
// ge:skill input
// ge:test tests/input_touch_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "input/state.hpp"

namespace ge::input {

enum class TouchKind : uint8_t { button, stick, lever, drag, flick, tap_hold };

// Whether a control is a gesture (claims no finger), and whether it reads as a button.
inline bool touch_gesture(TouchKind k) { return k == TouchKind::flick || k == TouchKind::tap_hold; }
inline bool touch_button(TouchKind k) { return k == TouchKind::button || touch_gesture(k); }

// A gesture's thresholds (above). Shares of the screen's height and seconds.
struct Gesture {
  float rise = 0.075f, window_s = 0.1f, speed = 1.5f, full = 6.0f, rearm = 0.03f;
  float hold_min_s = 0.05f, hold_max_s = 0.25f;
  float tap_s = 0.25f, gap_s = 0.35f;
};

struct TouchControl {
  std::string name;
  TouchKind kind = TouchKind::button;
  float x0 = 0, y0 = 0, x1 = 1, y1 = 1;  // the zone, 0..1 of the screen from the top left
  float radius = 0.12f;  // a stick's or a lever's travel, a share of the screen's height
  float start = 0;       // a lever's value where it is touched
  bool fixed = false;    // a stick centred on its zone rather than where it was touched
  bool trail = false;    // a stick whose centre trails a finger past its radius
  float floor = 0;       // a lever's lowest value (below 0: its second range)
  float dead = 0;        // a lever's band held at 0 before its second range, a share of `radius`
  std::string of{};      // a gesture's watched control
  Gesture gesture{};
};

struct TouchLayout {
  std::vector<TouchControl> controls;
  float aspect = 16.0f / 9.0f;            // the screen's width over its height
  int find(std::string_view name) const;  // -1 when there is none
};

struct TouchValue {
  bool held = false, pressed = false, released = false;
  uint16_t finger = 0;   // the finger that holds it
  float ax = 0, ay = 0;  // the anchor (screen 0..1)
  float x = 0, y = 0;    // a stick's push (-1..1, +y down), a lever's value in x
  float dx = 0, dy = 0;  // a drag's motion this frame, a share of the height
  float fx = 0, fy = 0;  // where the finger was last frame
  // A gesture's memory: the watched touch's times, the flick's strength (0..1) and when its
  // press ends, and the heights the flick looks back over (a ring, oldest at `at`).
  double down_s = -1e9, up_s = -1e9, until_s = 0;
  bool tapped = false, armed = true;
  float strength = 0, peak = 0;
  static constexpr int kTrack = 32;
  std::array<float, kTrack> heights{};
  std::array<double, kTrack> times{};
  int at = 0, n = 0;
};

struct TouchFrame {
  std::vector<TouchValue> c;  // one per control
};

TouchFrame make_touch_frame(const TouchLayout& layout);
// The frame's controls off `s`'s fingers; `now_s` is the clock the gestures read (any one clock,
// rising frame to frame).
void touch_update(const TouchLayout& layout, const InputState& s, TouchFrame& f, double now_s = 0);

}  // namespace ge::input
