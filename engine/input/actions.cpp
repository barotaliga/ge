// The action map's lookups and `evaluate`: every binding of every active context read, the
// blocking contexts' sources taken, the chords checked, and each kind's sources combined.
//
// ge:summary evaluate: the bindings read and combined into ActionValues, contexts and chords
// ge:keywords input actions evaluate combine strongest sum ramp chord blocking source_value
// ge:skill input
// ge:test tests/input_actions_test.cpp
#include "input/actions.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ge::input {

int ActionMap::find_action(std::string_view name) const {
  for (size_t i = 0; i < defs.size(); i++) {
    if (defs[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

Context* ActionMap::find_context(std::string_view name) {
  for (Context& c : contexts) {
    if (c.name == name) return &c;
  }
  return nullptr;
}

const Context* ActionMap::find_context(std::string_view name) const {
  return const_cast<ActionMap*>(this)->find_context(name);
}

bool ActionMap::set_active(std::string_view context, bool on) {
  Context* c = find_context(context);
  if (c) c->active = on;
  return c != nullptr;
}

Binding make_binding(uint16_t action, const Source& source) {
  Binding b;
  b.action = action;
  b.source = source;
  if (source.kind == SourceKind::role) {
    b.shape.inner = 0;
    b.shape.outer = 1;
  }
  return b;
}

ActionValues make_values(const ActionMap& map) {
  ActionValues v;
  v.v.resize(map.defs.size());
  return v;
}

namespace {

bool pooled(DeviceKind k) {
  return k == DeviceKind::keyboard || k == DeviceKind::mouse || k == DeviceKind::touch;
}

bool allowed(const Slot& slot, const Sources& from, const std::string& device) {
  if (!slot.active) return false;
  if (!device.empty() && slot.info.key != device) return false;
  if (from.only.empty()) return true;
  return std::find(from.only.begin(), from.only.end(), slot.info.id) != from.only.end();
}

bool joystick_like(DeviceKind k) { return k != DeviceKind::gamepad && !pooled(k); }

float halved(float v, int8_t half) {
  if (half > 0) return v > 0 ? v : 0;
  if (half < 0) return v < 0 ? -v : 0;
  return v;
}

// The strongest of `read(slot)` over the slots the source may come from.
template <class Pick, class Read>
float strongest(const InputState& s, const Sources& from, const std::string& device, Pick pick,
                Read read, DeviceId* by) {
  float best = 0;
  for (const Slot& slot : s.slots) {
    if (!allowed(slot, from, device) || !pick(slot.info.kind)) continue;
    const float v = read(slot);
    if (std::fabs(v) > std::fabs(best)) {
      best = v;
      if (by) *by = slot.info.id;
    }
  }
  return best;
}

const TouchValue* touch_value(const Source& src, const Sources& from, TouchKind* kind) {
  if (!from.layout || !from.touch) return nullptr;
  const int i = from.layout->find(src.name);
  if (i < 0 || static_cast<size_t>(i) >= from.touch->c.size()) return nullptr;
  if (kind) *kind = from.layout->controls[static_cast<size_t>(i)].kind;
  return &from.touch->c[static_cast<size_t>(i)];
}

bool is_pad(DeviceKind k) { return k == DeviceKind::gamepad; }

}  // namespace

bool is_digital(const Source& src, const Sources& from) {
  switch (src.kind) {
    case SourceKind::key:
    case SourceKind::mouse_button:
    case SourceKind::pad_button:
    case SourceKind::joy_button:
    case SourceKind::joy_hat:
      return true;
    case SourceKind::role:
      return is_button(static_cast<Role>(src.code));
    case SourceKind::touch: {
      TouchKind k = TouchKind::button;
      return touch_value(src, from, &k) && k == TouchKind::button;
    }
    default:
      return false;
  }
}

float source_value(const Source& src, const InputState& s, const Sources& from,
                   const std::string& device, DeviceId* by) {
  switch (src.kind) {
    case SourceKind::none:
      return 0;
    case SourceKind::key:
      if (!from.keyboard) return 0;
      if (by) *by = s.keyboard;
      return key_held(s, src.code) ? 1.0f : 0.0f;
    case SourceKind::mouse_button:
      if (!from.mouse) return 0;
      if (by) *by = s.mouse_id;
      return s.mouse.buttons.down(src.code) ? 1.0f : 0.0f;
    case SourceKind::mouse_motion:
      if (!from.mouse) return 0;
      if (by) *by = s.mouse_id;
      return halved(static_cast<float>(src.code == 0 ? s.mouse.dx : s.mouse.dy), src.half);
    case SourceKind::mouse_wheel:
      if (!from.mouse) return 0;
      if (by) *by = s.mouse_id;
      return halved(src.code == 0 ? s.mouse.wheel_x : s.mouse.wheel_y, src.half);
    case SourceKind::pad_button:
      return strongest(
          s, from, device, is_pad,
          [&](const Slot& sl) { return sl.buttons.down(src.code) ? 1.0f : 0.0f; }, by);
    case SourceKind::pad_axis:
      return strongest(
          s, from, device, is_pad,
          [&](const Slot& sl) {
            return src.code < kPadAxes ? halved(sl.axes[src.code], src.half) : 0.0f;
          },
          by);
    case SourceKind::pad_stick:  // its length; evaluate reads both axes
      return strongest(
          s, from, device, is_pad,
          [&](const Slot& sl) {
            const float x = sl.axes[src.code ? 2 : 0], y = sl.axes[src.code ? 3 : 1];
            return std::sqrt(x * x + y * y);
          },
          by);
    case SourceKind::joy_button:
      return strongest(
          s, from, device, joystick_like,
          [&](const Slot& sl) { return sl.buttons.down(src.code) ? 1.0f : 0.0f; }, by);
    case SourceKind::joy_axis:
      return strongest(
          s, from, device, joystick_like,
          [&](const Slot& sl) {
            return src.code < kMaxAxes ? halved(sl.axes[src.code], src.half) : 0.0f;
          },
          by);
    case SourceKind::joy_hat:
      return strongest(
          s, from, device, joystick_like,
          [&](const Slot& sl) {
            return src.code < kMaxHats && (sl.hats[src.code] & src.dir) ? 1.0f : 0.0f;
          },
          by);
    case SourceKind::gyro:
      return strongest(
          s, from, device, [](DeviceKind) { return true; },
          [&](const Slot& sl) { return src.code < 3 ? halved(sl.gyro[src.code], src.half) : 0; },
          by);
    case SourceKind::touch: {
      TouchKind k = TouchKind::button;
      const TouchValue* v = touch_value(src, from, &k);
      if (!v) return 0;
      if (by) *by = s.touch_id;
      if (k == TouchKind::button) return v->held ? 1.0f : 0.0f;
      if (k == TouchKind::drag) return halved(v->dx, src.half);
      return halved(v->x, src.half);
    }
    case SourceKind::role:
      return strongest(
          s, from, device, joystick_like,
          [&](const Slot& sl) {
            const DeviceRow row = row_for(from.db, sl.info);
            return halved(role_value(row, sl, static_cast<Role>(src.code)), src.half);
          },
          by);
  }
  return 0;
}

bool source_tapped(const Source& src, const InputState& s, const Sources& from,
                   const std::string& device) {
  switch (src.kind) {
    case SourceKind::key:
      return from.keyboard && key_pressed(s, src.code);
    case SourceKind::mouse_button:
      return from.mouse && s.mouse.buttons.went_down(src.code);
    case SourceKind::pad_button:
    case SourceKind::joy_button:
      for (const Slot& sl : s.slots) {
        const bool kind_ok =
            src.kind == SourceKind::pad_button ? is_pad(sl.info.kind) : joystick_like(sl.info.kind);
        if (allowed(sl, from, device) && kind_ok && sl.buttons.went_down(src.code)) return true;
      }
      return false;
    case SourceKind::joy_hat:
      for (const Slot& sl : s.slots) {
        if (allowed(sl, from, device) && joystick_like(sl.info.kind) && src.code < kMaxHats &&
            (sl.hats_pressed[src.code] & src.dir)) {
          return true;
        }
      }
      return false;
    case SourceKind::touch: {
      const TouchValue* v = touch_value(src, from, nullptr);
      return v && v->pressed;
    }
    default:
      return false;
  }
}

namespace {

// The sources a blocking context has taken this frame.
struct Taken {
  std::array<const Source*, 256> src{};
  size_t n = 0;
  bool has(const Source& s) const {
    for (size_t i = 0; i < n; i++) {
      if (*src[i] == s) return true;
    }
    return false;
  }
  void add(const Source& s) {
    if (n < src.size()) src[n++] = &s;
  }
};

struct Acc {
  bool held = false, tapped = false;
  float key_x = 0, key_y = 0;  // the keys' summed push
  float an_x = 0, an_y = 0;    // the strongest analog
  bool any_key = false;
  const Ramp* ramp = nullptr;
  double dx = 0, dy = 0;  // a delta's sum
  DeviceId by = 0;
};

float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void read_binding(const ActionDef& def, const Binding& b, const InputState& s, const Sources& from,
                  double dt, bool was_held, Acc& a) {
  DeviceId by = 0;
  const bool dig = is_digital(b.source, from);
  // Both axes of a stick-shaped source.
  const auto stick_xy = [&](float& x, float& y) {
    x = y = 0;
    if (b.source.kind == SourceKind::pad_stick) {
      strongest(
          s, from, b.device, is_pad,
          [&](const Slot& sl) {
            float sx = sl.axes[b.source.code ? 2 : 0], sy = sl.axes[b.source.code ? 3 : 1];
            shape_stick(sx, sy, b.stick);
            const float len = std::sqrt(sx * sx + sy * sy);
            if (len > std::sqrt(x * x + y * y)) {
              x = sx;
              y = sy;
            }
            return len;
          },
          &by);
      return true;
    }
    if (b.source.kind == SourceKind::touch) {
      TouchKind k = TouchKind::button;
      const TouchValue* v = touch_value(b.source, from, &k);
      if (v && k == TouchKind::stick) {
        x = v->x;
        y = v->y;
        shape_stick(x, y, b.stick);
        by = s.touch_id;
        return true;
      }
    }
    return false;
  };

  const float raw = source_value(b.source, s, from, b.device, &by);
  switch (def.kind) {
    case ActionKind::button: {
      const bool on = dig ? raw != 0 : digital(was_held, raw, b.threshold);
      if (on) a.held = true;
      if (dig && source_tapped(b.source, s, from, b.device)) a.tapped = true;
      if (on && !a.by) a.by = by;
      return;
    }
    case ActionKind::axis:
    case ActionKind::stick: {
      float x = 0, y = 0;
      if (dig) {
        if (!a.ramp) a.ramp = &b.ramp;
        a.any_key = true;
        if (raw != 0) {
          (b.component ? a.key_y : a.key_x) += b.value;
          if (!a.by) a.by = by;
        }
        return;
      }
      if (def.kind == ActionKind::stick && stick_xy(x, y)) {
        x *= b.value;
        y *= b.value;
        if (x * x + y * y > a.an_x * a.an_x + a.an_y * a.an_y) {
          a.an_x = x;
          a.an_y = y;
          if (x != 0 || y != 0) a.by = by;
        }
        return;
      }
      float v = 0;
      if (b.source.kind == SourceKind::pad_stick) {
        float sx = 0, sy = 0;
        stick_xy(sx, sy);
        v = (b.component ? sy : sx) * b.value;
      } else {
        v = shape_axis(raw, b.shape) * b.value;
      }
      float& slot = b.component ? a.an_y : a.an_x;
      if (std::fabs(v) > std::fabs(slot)) {
        slot = v;
        a.by = by;
      }
      return;
    }
    case ActionKind::delta: {
      double dx = 0, dy = 0;
      float x = 0, y = 0;
      if (b.source.kind == SourceKind::mouse_motion || b.source.kind == SourceKind::mouse_wheel) {
        (b.component ? dy : dx) = static_cast<double>(raw) * b.value;
      } else if (b.source.kind == SourceKind::gyro) {
        (b.component ? dy : dx) = static_cast<double>(raw) * b.value * dt;
      } else if (b.source.kind == SourceKind::touch && !dig &&
                 touch_value(b.source, from, nullptr) && !stick_xy(x, y)) {
        TouchKind k = TouchKind::button;
        const TouchValue* tv = touch_value(b.source, from, &k);
        if (k == TouchKind::drag) {
          dx = static_cast<double>(tv->dx) * b.value;
          dy = static_cast<double>(tv->dy) * b.value;
        } else {
          (b.component ? dy : dx) = static_cast<double>(tv->x) * b.value * b.rate * dt;
        }
      } else if (stick_xy(x, y)) {
        dx = static_cast<double>(x) * b.value * b.rate * dt;
        dy = static_cast<double>(y) * b.value * b.rate * dt;
      } else if (dig) {
        (b.component ? dy : dx) = (raw != 0 ? 1.0 : 0.0) * b.value * b.rate * dt;
      } else {
        (b.component ? dy : dx) =
            static_cast<double>(shape_axis(raw, b.shape)) * b.value * b.rate * dt;
      }
      a.dx += dx;
      a.dy += dy;
      if ((dx != 0 || dy != 0) && !a.by) a.by = by;
      return;
    }
  }
}

}  // namespace

void evaluate(const ActionMap& map, const InputState& s, double dt_s, ActionValues& out,
              const Sources& from) {
  if (out.v.size() != map.defs.size()) out.v.resize(map.defs.size());
  std::array<Acc, 256> stack_acc{};
  std::vector<Acc> heap_acc;
  Acc* acc = stack_acc.data();
  if (map.defs.size() > stack_acc.size()) {
    heap_acc.resize(map.defs.size());
    acc = heap_acc.data();
  }
  Taken taken;
  for (const Context& ctx : map.contexts) {
    if (!ctx.active) continue;
    for (const Binding& b : ctx.bindings) {
      if (b.action >= map.defs.size() || taken.has(b.source)) continue;
      if (b.with.kind != SourceKind::none && source_value(b.with, s, from, b.device) < 0.5f) {
        continue;
      }
      read_binding(map.defs[b.action], b, s, from, dt_s, out.v[b.action].held, acc[b.action]);
    }
    if (ctx.blocking) {
      for (const Binding& b : ctx.bindings) taken.add(b.source);
    }
  }
  for (size_t i = 0; i < map.defs.size(); i++) {
    const ActionDef& def = map.defs[i];
    const Acc& a = acc[i];
    ActionValue& v = out.v[i];
    const bool was = v.held;
    bool held = false;
    switch (def.kind) {
      case ActionKind::button:
        held = a.held;
        v.x = held ? 1.0f : 0.0f;
        v.y = 0;
        break;
      case ActionKind::axis:
      case ActionKind::stick: {
        const Ramp r = a.ramp ? *a.ramp : Ramp{};
        const float lo = def.lever ? 0.0f : -1.0f;
        v.key_x = ramp_toward(v.key_x, clamp(a.key_x, lo, 1), dt_s, r);
        v.key_y = def.kind == ActionKind::stick
                      ? ramp_toward(v.key_y, clamp(a.key_y, -1, 1), dt_s, r)
                      : 0.0f;
        float kx = v.key_x, ky = v.key_y;
        if (def.kind == ActionKind::stick) {
          const float len = std::sqrt(kx * kx + ky * ky);
          if (len > 1) {
            kx /= len;
            ky /= len;
          }
          const bool analog = a.an_x * a.an_x + a.an_y * a.an_y > kx * kx + ky * ky;
          v.x = analog ? a.an_x : kx;
          v.y = analog ? a.an_y : ky;
          held = v.x * v.x + v.y * v.y > 0.25f;
        } else {
          v.x = clamp(std::fabs(a.an_x) > std::fabs(kx) ? a.an_x : kx, lo, 1);
          v.y = 0;
          held = std::fabs(v.x) > 0.5f;
        }
        break;
      }
      case ActionKind::delta:
        v.x = static_cast<float>(a.dx);
        v.y = static_cast<float>(a.dy);
        held = v.x != 0 || v.y != 0;
        break;
    }
    v.pressed = held && !was;
    v.released = !held && was;
    if (a.tapped && !held && !was) v.pressed = v.released = true;
    v.held = held;
    v.device = a.by;
  }
}

}  // namespace ge::input
