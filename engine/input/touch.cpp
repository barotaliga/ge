// The touch controls' frame: fingers claimed by zones, sticks, levers and drags read.
//
// ge:summary The touch frame: fingers claimed by their zones and each control read
// ge:keywords input touch touch_update claim finger zone stick lever drag
// ge:skill input
// ge:test tests/input_touch_test.cpp
#include "input/touch.hpp"

#include <cmath>

namespace ge::input {

int TouchLayout::find(std::string_view name) const {
  for (size_t i = 0; i < controls.size(); i++) {
    if (controls[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

TouchFrame make_touch_frame(const TouchLayout& layout) {
  TouchFrame f;
  f.c.resize(layout.controls.size());
  return f;
}

void touch_update(const TouchLayout& layout, const InputState& s, TouchFrame& f) {
  if (f.c.size() != layout.controls.size()) f.c.resize(layout.controls.size());
  for (TouchValue& v : f.c) {
    v.pressed = v.released = false;
    v.dx = v.dy = 0;
  }
  // A held control follows its finger, or lets go when the finger has lifted.
  for (size_t i = 0; i < layout.controls.size(); i++) {
    TouchValue& v = f.c[i];
    if (!v.held) continue;
    const Finger* finger = nullptr;
    for (const Finger& x : s.fingers) {
      if ((x.active || x.ended) && x.id == v.finger) finger = &x;
    }
    if (!finger || !finger->active) {
      v.held = false;
      v.released = true;
      v.x = v.y = 0;
      continue;
    }
    const TouchControl& c = layout.controls[i];
    v.dx = finger->x * layout.aspect - v.fx * layout.aspect;
    v.dy = finger->y - v.fy;
    v.fx = finger->x;
    v.fy = finger->y;
    const float r = c.radius > 0 ? c.radius : 1;
    if (c.kind == TouchKind::stick) {
      float px = (finger->x - v.ax) * layout.aspect / r, py = (finger->y - v.ay) / r;
      const float len = std::sqrt(px * px + py * py);
      if (len > 1) {
        px /= len;
        py /= len;
      }
      v.x = px;
      v.y = py;
    } else if (c.kind == TouchKind::lever) {
      const float t = c.start + (v.ay - finger->y) / r;
      v.x = t < 0 ? 0 : (t > 1 ? 1 : t);
    }
  }
  // A finger that touched down this frame goes to the first free control whose zone holds it.
  for (const Finger& finger : s.fingers) {
    if (!finger.began) continue;
    bool taken = false;
    for (const TouchValue& v : f.c) taken = taken || (v.held && v.finger == finger.id);
    for (size_t i = 0; i < layout.controls.size() && !taken; i++) {
      const TouchControl& c = layout.controls[i];
      TouchValue& v = f.c[i];
      if (v.held || finger.x0 < c.x0 || finger.x0 > c.x1 || finger.y0 < c.y0 || finger.y0 > c.y1) {
        continue;
      }
      v.held = v.pressed = true;
      v.finger = finger.id;
      v.ax = c.fixed ? (c.x0 + c.x1) / 2 : finger.x0;
      v.ay = c.fixed ? (c.y0 + c.y1) / 2 : finger.y0;
      v.fx = finger.x;
      v.fy = finger.y;
      v.x = c.kind == TouchKind::lever ? c.start : 0;
      v.y = 0;
      if (c.kind == TouchKind::stick && c.fixed) {
        const float r = c.radius > 0 ? c.radius : 1;
        v.x = (finger.x - v.ax) * layout.aspect / r;
        v.y = (finger.y - v.ay) / r;
      }
      taken = true;
      // A finger that lifted in the same frame it landed is a tap: pressed and released.
      if (!finger.active) {
        v.held = false;
        v.released = true;
      }
    }
  }
}

}  // namespace ge::input
