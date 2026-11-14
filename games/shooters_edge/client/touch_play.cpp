// The game played by touch, defined (touch_play.hpp).
//
// ge:summary The game played by touch defined: the layout, the moves, the controls drawn
// ge:keywords touch touch_play touch_moves draw_touch
// ge:skill input shooter
// ge:test tests/shooters_edge_touch_play_test.cpp
#include "shooters_edge/client/touch_play.hpp"

#include <algorithm>

namespace shooters_edge::client {

namespace in = ge::input;

namespace {

constexpr float kStickReach = 0.12f;  // the stick's travel, a share of the screen's height

const in::TouchValue* value(const TouchPlay& t, const char* name) {
  const int i = t.layout.find(name);
  return i < 0 || static_cast<size_t>(i) >= t.frame.c.size() ? nullptr
                                                             : &t.frame.c[static_cast<size_t>(i)];
}

}  // namespace

TouchPlay touch_play(float aspect) {
  TouchPlay t;
  t.layout.aspect = aspect;
  // Buttons first: a finger belongs to the first zone it lands in.
  const in::TouchControl weapon{"weapon", in::TouchKind::button, 0.82f, 0, 1, 0.2f};
  const in::TouchControl jump{"jump", in::TouchKind::button, 0.82f, 0.36f, 1, 0.6f};
  const in::TouchControl fire{"fire", in::TouchKind::button, 0.82f, 0.6f, 1, 1};
  in::TouchControl walk{"walk", in::TouchKind::stick, 0, 0.2f, 0.45f, 1, kStickReach};
  walk.trail = true;
  const in::TouchControl look{"look", in::TouchKind::drag, 0.45f, 0, 1, 1};
  t.layout.controls = {weapon, jump, fire, walk, look};
  t.frame = in::make_touch_frame(t.layout);
  return t;
}

void touch_moves(TouchPlay& t, const in::InputState& s, double now_s, Moves& m, float& yaw,
                 float& pitch) {
  yaw = pitch = 0;
  for (const in::Finger& f : s.fingers) t.seen = t.seen || f.active || f.ended;
  in::touch_update(t.layout, s, t.frame, now_s);
  if (const in::TouchValue* w = value(t, "walk"); w != nullptr && w->held) {
    m.forward = m.forward || w->y < -kTouchPush;
    m.back = m.back || w->y > kTouchPush;
    m.moveleft = m.moveleft || w->x < -kTouchPush;
    m.moveright = m.moveright || w->x > kTouchPush;
  }
  if (const in::TouchValue* l = value(t, "look")) {
    yaw = l->dx * kTouchLookDegrees;
    pitch = l->dy * kTouchLookDegrees;
  }
  if (const in::TouchValue* f = value(t, "fire")) m.attack = m.attack || f->held;
  if (const in::TouchValue* j = value(t, "jump")) m.jump = m.jump || j->held;
  if (const in::TouchValue* w = value(t, "weapon"); w != nullptr && w->pressed)
    m.impulse = kNextWeaponImpulse;
}

void draw_touch(const TouchPlay& t, ge::ui::DrawList& d, int w, int h) {
  if (!t.seen) return;
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  for (size_t i = 0; i < t.layout.controls.size() && i < t.frame.c.size(); i++) {
    const in::TouchControl& c = t.layout.controls[i];
    const in::TouchValue& v = t.frame.c[i];
    if (c.kind == in::TouchKind::button) {
      const float r = std::min((c.x1 - c.x0) * fw, (c.y1 - c.y0) * fh) * 0.35f;
      const float cx = (c.x0 + c.x1) * 0.5f * fw, cy = (c.y0 + c.y1) * 0.5f * fh;
      d.rect({cx - r, cy - r, 2 * r, 2 * r}, ge::ui::Color{1, 1, 1, v.held ? 0.45f : 0.15f}, r);
    } else if (c.kind == in::TouchKind::stick && v.held) {
      const float r = c.radius * fh;
      const float ax = v.ax * fw, ay = v.ay * fh;
      d.rect({ax - r, ay - r, 2 * r, 2 * r}, ge::ui::Color{1, 1, 1, 0.15f}, r);
      const float k = r * 0.4f, kx = ax + v.x * r, ky = ay + v.y * r;
      d.rect({kx - k, ky - k, 2 * k, 2 * k}, ge::ui::Color{1, 1, 1, 0.45f}, k);
    }
  }
}

}  // namespace shooters_edge::client
