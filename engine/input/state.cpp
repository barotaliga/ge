// The input state's fold: the queue's plug and unplug, the slots, and each event type applied.
//
// ge:summary The fold of a frame's events into the input state, event type by event type
// ge:keywords input state apply update fold slot plug unplug release-on-unplug edges
// ge:skill input
// ge:test tests/input_state_test.cpp
#include "input/state.hpp"

#include <cstdio>

namespace ge::input {

namespace {

void set_edge(Edges64& e, int i, bool down) {
  if (i < 0 || i >= 64) return;
  const uint64_t bit = uint64_t{1} << i;
  if (down) {
    if (!(e.held & bit)) e.pressed |= bit;
    e.held |= bit;
  } else {
    if (e.held & bit) e.released |= bit;
    e.held &= ~bit;
  }
}

void mark_used(InputState& s, DeviceId id, DeviceKind kind) {
  s.last_used = id;
  s.last_kind = kind;
}

template <size_t N>
void apply_finger(std::array<Finger, N>& fingers, const Event& e) {
  constexpr int n = static_cast<int>(N);
  const auto phase = static_cast<TouchPhase>(e.aux);
  Finger* f = nullptr;
  for (int i = 0; i < n; i++) {
    if (fingers[i].active && fingers[i].id == e.code) f = &fingers[i];
  }
  if (!f && phase == TouchPhase::down) {
    for (int i = 0; i < n && !f; i++) {
      if (!fingers[i].active && !fingers[i].ended) f = &fingers[i];  // a lift this frame stays read
    }
    if (!f) return;  // more fingers than the screen keeps
    *f = {};
    f->id = e.code;
    f->active = f->began = true;
    f->x0 = e.v[0];
    f->y0 = e.v[1];
  }
  if (!f) return;
  f->x = e.v[0];
  f->y = e.v[1];
  f->force = e.v[2];
  if (phase == TouchPhase::up) {
    f->active = false;
    f->ended = true;
  }
}

}  // namespace

void Queue::plug(uint64_t t_ns, const DeviceInfo& info) {
  Event e{t_ns, info.id, EventType::device_added, 0, static_cast<uint16_t>(plugged.size()), {}};
  plugged.push_back(info);
  events.push_back(e);
}

void Queue::unplug(uint64_t t_ns, DeviceId id) {
  events.push_back({t_ns, id, EventType::device_removed, 0, 0, {}});
}

Slot* find_slot(InputState& s, DeviceId id) {
  if (id == 0) return nullptr;
  for (Slot& slot : s.slots) {
    if (slot.active && slot.info.id == id) return &slot;
  }
  return nullptr;
}

const Slot* find_slot(const InputState& s, DeviceId id) {
  return find_slot(const_cast<InputState&>(s), id);
}

void begin_frame(InputState& s) {
  s.keys_pressed.reset();
  s.keys_released.reset();
  s.mouse.dx = s.mouse.dy = 0;
  s.mouse.wheel_x = s.mouse.wheel_y = 0;
  s.mouse.buttons.pressed = s.mouse.buttons.released = 0;
  for (Slot& slot : s.slots) {
    slot.buttons.pressed = slot.buttons.released = 0;
    slot.hats_pressed.fill(0);
    for (Finger& f : slot.pad_fingers) f.began = f.ended = false;
  }
  for (Finger& f : s.fingers) f.began = f.ended = false;
  s.text_n = 0;
  s.added_n = s.removed_n = 0;
}

void apply(InputState& s, const Event& e, const std::vector<DeviceInfo>& plugged) {
  s.t_ns = e.t_ns;
  switch (e.type) {
    case EventType::device_added: {
      if (e.code >= plugged.size()) return;
      Slot* slot = find_slot(s, e.device);
      for (size_t i = 0; !slot && i < s.slots.size(); i++) {
        if (!s.slots[i].active) slot = &s.slots[i];
      }
      if (!slot) {
        std::fprintf(stderr,
                     "input: more than %d devices plugged in; device %u is ignored until one "
                     "is unplugged (raise kMaxDevices in engine/input/state.hpp; skill: input)\n",
                     kMaxDevices, e.device);
        return;
      }
      *slot = {};
      slot->active = true;
      slot->info = plugged[e.code];
      if (slot->info.kind == DeviceKind::keyboard) s.keyboard = e.device;
      if (slot->info.kind == DeviceKind::mouse) s.mouse_id = e.device;
      if (slot->info.kind == DeviceKind::touch) s.touch_id = e.device;
      if (s.added_n < kMaxDevices) s.added[s.added_n++] = e.device;
      return;
    }
    case EventType::device_removed: {
      Slot* slot = find_slot(s, e.device);
      if (!slot) return;
      slot->buttons.released |= slot->buttons.held;
      slot->buttons.held = 0;
      slot->axes.fill(0);
      slot->hats.fill(0);
      slot->active = false;
      if (s.removed_n < kMaxDevices) s.removed[s.removed_n++] = e.device;
      if (s.last_used == e.device) s.last_used = 0;
      return;
    }
    case EventType::key: {
      if (e.code >= kKeys || e.aux) return;  // a repeat is not a press
      const bool down = e.v[0] != 0;
      if (down) {
        if (!s.keys_held[e.code]) s.keys_pressed[e.code] = true;
        mark_used(s, e.device, DeviceKind::keyboard);
      } else if (s.keys_held[e.code]) {
        s.keys_released[e.code] = true;
      }
      s.keys_held[e.code] = down;
      return;
    }
    case EventType::text:
      if (s.text_n < kMaxText) s.text[s.text_n++] = static_cast<uint32_t>(e.v[0]);
      return;
    case EventType::mouse_motion:
      s.mouse.dx += e.v[0];
      s.mouse.dy += e.v[1];
      s.mouse.x = e.v[2];
      s.mouse.y = e.v[3];
      s.mouse.raw = e.aux != 0;
      if (e.v[0] != 0 || e.v[1] != 0) mark_used(s, e.device, DeviceKind::mouse);
      return;
    case EventType::mouse_button:
      set_edge(s.mouse.buttons, e.code, e.v[0] != 0);
      if (e.v[0] != 0) mark_used(s, e.device, DeviceKind::mouse);
      return;
    case EventType::mouse_wheel:
      s.mouse.wheel_x += e.v[0];
      s.mouse.wheel_y += e.v[1];
      mark_used(s, e.device, DeviceKind::mouse);
      return;
    case EventType::pad_button:
    case EventType::joy_button: {
      Slot* slot = find_slot(s, e.device);
      if (!slot) return;
      set_edge(slot->buttons, e.code, e.v[0] != 0);
      if (e.v[0] != 0) mark_used(s, e.device, slot->info.kind);
      return;
    }
    case EventType::pad_axis:
    case EventType::joy_axis: {
      Slot* slot = find_slot(s, e.device);
      if (!slot || e.code >= kMaxAxes) return;
      const float before = slot->axes[e.code];
      slot->axes[e.code] = e.v[0];
      const auto past_half = [](float v) { return v > 0.5f || v < -0.5f; };
      if (past_half(e.v[0]) && !past_half(before)) mark_used(s, e.device, slot->info.kind);
      return;
    }
    case EventType::joy_hat: {
      Slot* slot = find_slot(s, e.device);
      if (!slot || e.code >= kMaxHats) return;
      const auto now = static_cast<uint8_t>(e.v[0]);
      const auto went = static_cast<uint8_t>(now & ~slot->hats[e.code]);
      slot->hats_pressed[e.code] |= went;
      slot->hats[e.code] = now;
      if (went) mark_used(s, e.device, slot->info.kind);
      return;
    }
    case EventType::sensor: {
      Slot* slot = find_slot(s, e.device);
      if (!slot) return;
      auto& v = e.code == 0 ? slot->gyro : slot->accel;
      v = {e.v[0], e.v[1], e.v[2]};
      return;
    }
    case EventType::touch:
      apply_finger(s.fingers, e);
      if (static_cast<TouchPhase>(e.aux) == TouchPhase::down) {
        mark_used(s, e.device, DeviceKind::touch);
      }
      return;
    case EventType::pad_touch: {
      Slot* slot = find_slot(s, e.device);
      if (!slot) return;
      apply_finger(slot->pad_fingers, e);
      return;
    }
    case EventType::battery: {
      Slot* slot = find_slot(s, e.device);
      if (slot) slot->battery = e.v[0];
      return;
    }
  }
}

void update(InputState& s, const Queue& q) {
  begin_frame(s);
  for (const Event& e : q.events) apply(s, e, q.plugged);
}

int count_kind(const InputState& s, DeviceKind kind) {
  int n = 0;
  for (const Slot& slot : s.slots) n += slot.active && slot.info.kind == kind;
  return n;
}

}  // namespace ge::input
