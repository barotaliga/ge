// THE TOUCH CONTROLS: a phone's or a tablet's screen read as controls a binding can name
// (`touch:<name>`). A layout is a list of controls, each a ZONE of the screen (0..1 from the top
// left); a finger belongs to the first control whose zone it touched down in, and keeps it
// until it lifts, wherever it slides.
//
//   button  held while its finger is down
//   stick   a floating stick: its centre is where the finger touched down; the push is the
//           finger's offset over `radius` (a share of the screen's height, so it is round on
//           any screen), at most 1. A `fixed` stick's centre is its zone's centre instead.
//   lever   one value 0..1: `start` where the finger touched down, rising as it slides up and
//           falling as it slides down, one full travel over `radius`. A lever anchored full
//           (`start = 1`) is a throttle held by default that a thumb eases off.
//   drag    the finger's motion this frame (a share of the height), for a look or a camera
//
// The frame is computed once (`touch_update`) after the input state's update and before the
// actions are evaluated, which read it through `Sources::touch`.
//
// ge:summary Touch controls: zones, floating and fixed sticks, buttons, levers, drags
// ge:keywords input touch touchscreen mobile phone tablet virtual-stick floating-stick thumb
// ge:keywords lever slider drag look zone touch-layout touch-frame touch_update button
// ge:skill input
// ge:test tests/input_touch_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "input/state.hpp"

namespace ge::input {

enum class TouchKind : uint8_t { button, stick, lever, drag };

struct TouchControl {
  std::string name;
  TouchKind kind = TouchKind::button;
  float x0 = 0, y0 = 0, x1 = 1, y1 = 1;  // the zone, 0..1 of the screen from the top left
  float radius = 0.12f;  // a stick's or a lever's travel, a share of the screen's height
  float start = 0;       // a lever's value where it is touched
  bool fixed = false;    // a stick centred on its zone rather than where it was touched
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
};

struct TouchFrame {
  std::vector<TouchValue> c;  // one per control
};

TouchFrame make_touch_frame(const TouchLayout& layout);
void touch_update(const TouchLayout& layout, const InputState& s, TouchFrame& f);

}  // namespace ge::input
