// The touch controls' frame: fingers claimed by zones, sticks, levers and drags read, then the
// gestures read off the controls they watch.
//
// ge:summary The touch frame: fingers claimed by their zones and each control read
// ge:keywords input touch touch_update claim finger zone stick lever drag flick tap-hold
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

namespace {

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// A lever's value for a thumb at `y`: the first range from `start`, then past the dead band the
// second, down to `floor`.
float lever_value(const TouchControl& c, float ay, float y, float r) {
  const float t = c.start + (ay - y) / r;
  if (t >= 0 || c.floor >= 0) return t > 1 ? 1 : (t < 0 ? 0 : t);
  const float past = -t - c.dead;
  if (past <= 0) return 0;
  return past < -c.floor ? -past : c.floor;
}

// The flick's look back: the finger at `y` at `t`, kept while it is inside the window.
void track(TouchValue& g, float y, double t, float window_s) {
  g.ys[static_cast<size_t>((g.at + g.n) % TouchValue::kTrack)] = y;
  g.ts[static_cast<size_t>((g.at + g.n) % TouchValue::kTrack)] = t;
  if (g.n < TouchValue::kTrack) {
    ++g.n;
  } else {
    g.at = (g.at + 1) % TouchValue::kTrack;
  }
  while (g.n > 1 && t - g.ts[static_cast<size_t>(g.at)] > window_s) {
    g.at = (g.at + 1) % TouchValue::kTrack;
    --g.n;
  }
}

void flick(const TouchControl& c, const TouchValue& w, TouchValue& g, double now) {
  const Gesture& k = c.gesture;
  if (w.pressed) {
    g.n = g.at = 0;
    g.armed = true;
    g.peak = w.fy;
    track(g, w.fy, now, k.window_s);
  } else if (w.held) {
    track(g, w.fy, now, k.window_s);
    if (!g.armed) {
      g.peak = w.fy < g.peak ? w.fy : g.peak;
      if (w.fy - g.peak >= k.rearm) g.armed = true;
    } else {
      // The lowest point in the window (the largest y: down the screen), and how fast the finger
      // rose from it to here.
      int low = 0;
      for (int i = 1; i < g.n; i++) {
        if (g.ys[static_cast<size_t>((g.at + i) % TouchValue::kTrack)] >
            g.ys[static_cast<size_t>((g.at + low) % TouchValue::kTrack)]) {
          low = i;
        }
      }
      const size_t li = static_cast<size_t>((g.at + low) % TouchValue::kTrack);
      const float rise = g.ys[li] - w.fy;
      const double dt = now - g.ts[li] > 1e-3 ? now - g.ts[li] : 1e-3;
      const float speed = static_cast<float>(rise / dt);
      if (rise >= k.rise && speed >= k.speed) {
        g.armed = false;
        g.peak = w.fy;
        g.strength = clamp01((speed - k.speed) / (k.full - k.speed));
        g.held = g.pressed = true;
        g.until_s = now + k.hold_min_s + (k.hold_max_s - k.hold_min_s) * g.strength;
      }
    }
  }
  if (g.held && !g.pressed && now >= g.until_s) {
    g.held = false;
    g.released = true;
  }
}

void tap_hold(const TouchControl& c, const TouchValue& w, TouchValue& g, double now) {
  const Gesture& k = c.gesture;
  if (w.pressed) {
    const bool loading = g.tapped && now - g.up_s <= k.gap_s;
    g.tapped = false;
    g.down_s = now;
    if (loading) g.held = g.pressed = true;
  }
  if (w.released) {
    if (g.held) {
      g.held = false;
      g.released = true;
      g.tapped = false;
    } else {
      g.tapped = !g.pressed && now - g.down_s <= k.tap_s;
    }
    g.up_s = now;
  }
}

}  // namespace

void touch_update(const TouchLayout& layout, const InputState& s, TouchFrame& f, double now_s) {
  if (f.c.size() != layout.controls.size()) f.c.resize(layout.controls.size());
  for (TouchValue& v : f.c) {
    v.pressed = v.released = false;
    v.dx = v.dy = 0;
  }
  // A held control follows its finger, or lets go when the finger has lifted.
  for (size_t i = 0; i < layout.controls.size(); i++) {
    TouchValue& v = f.c[i];
    if (!v.held || touch_gesture(layout.controls[i].kind)) continue;
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
        if (c.trail) {  // the centre dragged to one radius behind the finger
          v.ax = finger->x - px * r / layout.aspect;
          v.ay = finger->y - py * r;
        }
      }
      v.x = px;
      v.y = py;
    } else if (c.kind == TouchKind::lever) {
      v.x = lever_value(c, v.ay, finger->y, r);
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
      if (v.held || touch_gesture(c.kind) || finger.x0 < c.x0 || finger.x0 > c.x1 ||
          finger.y0 < c.y0 || finger.y0 > c.y1) {
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
  // The gestures, off the controls they watch (a gesture on a gesture reads nothing).
  for (size_t i = 0; i < layout.controls.size(); i++) {
    const TouchControl& c = layout.controls[i];
    if (!touch_gesture(c.kind)) continue;
    const int w = layout.find(c.of);
    if (w < 0 || static_cast<size_t>(w) >= f.c.size()) continue;
    const TouchKind wk = layout.controls[static_cast<size_t>(w)].kind;
    if (touch_gesture(wk)) continue;
    if (c.kind == TouchKind::flick) {
      flick(c, f.c[static_cast<size_t>(w)], f.c[i], now_s);
    } else {
      tap_hold(c, f.c[static_cast<size_t>(w)], f.c[i], now_s);
    }
  }
}

}  // namespace ge::input
