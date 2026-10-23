// Rebinding: the capture's rests and its watch, the conflicts, the rebind.
//
// ge:summary The capture's rest and watch, conflicts and rebind
// ge:keywords input rebind capture rest conflicts
// ge:skill input
// ge:test tests/input_actions_test.cpp
#include "input/rebind.hpp"

#include <cmath>

#include "input/names.hpp"

namespace ge::input {

void begin_capture(Capture& c, const InputState& s, ActionKind for_kind) {
  c = Capture{};
  c.active = true;
  c.for_kind = for_kind;
  for (size_t i = 0; i < s.slots.size(); i++) {
    c.rest[i].id = s.slots[i].active ? s.slots[i].info.id : 0;
    c.rest[i].axes = s.slots[i].axes;
  }
}

CaptureResult poll_capture(Capture& c, const InputState& s, Source& out, std::string& device) {
  if (!c.active) return CaptureResult::waiting;
  device.clear();
  const auto got = [&](SourceKind k, unsigned code) {
    out = Source{};
    out.kind = k;
    out.code = static_cast<uint16_t>(code);
    c.active = false;
    return CaptureResult::captured;
  };
  if (key_pressed(s, key::escape)) {
    c.active = false;
    return CaptureResult::cancelled;
  }
  for (unsigned k = 0; k < kKeys; k++) {
    if (s.keys_pressed[k]) return got(SourceKind::key, k);
  }
  for (unsigned b = 0; b < kMouseButtons; b++) {
    if (s.mouse.buttons.went_down(static_cast<int>(b))) return got(SourceKind::mouse_button, b);
  }
  if (c.for_kind == ActionKind::delta || c.for_kind == ActionKind::axis) {
    c.mouse_x += s.mouse.dx;
    c.mouse_y += s.mouse.dy;
    if (std::fabs(c.mouse_x) > c.mouse_travel || std::fabs(c.mouse_y) > c.mouse_travel) {
      return got(SourceKind::mouse_motion, std::fabs(c.mouse_y) > std::fabs(c.mouse_x));
    }
  }
  for (size_t i = 0; i < s.slots.size(); i++) {
    const Slot& slot = s.slots[i];
    if (!slot.active) continue;
    const bool pad = slot.info.kind == DeviceKind::gamepad;
    const bool joy = !pad && slot.info.kind != DeviceKind::keyboard &&
                     slot.info.kind != DeviceKind::mouse && slot.info.kind != DeviceKind::touch;
    if (!pad && !joy) continue;
    for (int b = 0; b < kMaxButtons; b++) {
      if (slot.buttons.went_down(b)) {
        if (joy) device = slot.info.key;
        return got(pad ? SourceKind::pad_button : SourceKind::joy_button, static_cast<unsigned>(b));
      }
    }
    for (int h = 0; h < kMaxHats && joy; h++) {
      for (int d = 0; d < 4; d++) {
        if (slot.hats_pressed[h] & (1 << d)) {
          device = slot.info.key;
          got(SourceKind::joy_hat, static_cast<unsigned>(h));
          out.dir = static_cast<uint8_t>(1 << d);
          return CaptureResult::captured;
        }
      }
    }
    const Capture::Rest* rest = nullptr;
    for (const Capture::Rest& r : c.rest) {
      if (r.id == slot.info.id) rest = &r;
    }
    const int n = pad ? kPadAxes : kMaxAxes;
    for (int a = 0; a < n; a++) {
      const float from = rest ? rest->axes[a] : 0.0f;
      const float moved = slot.axes[a] - from;
      if (std::fabs(moved) <= 0.5f) continue;
      if (pad && c.for_kind == ActionKind::stick && a < 4) {
        return got(SourceKind::pad_stick, a >= 2 ? 1u : 0u);
      }
      if (joy) device = slot.info.key;
      got(pad ? SourceKind::pad_axis : SourceKind::joy_axis, static_cast<unsigned>(a));
      // A trigger (0..1) or a pedal resting at one end is bound whole; a stick's axis moved
      // one way, bound for a button, is that half.
      const bool from_end = std::fabs(from) > 0.9f;
      const bool trigger = pad && a >= 4;
      if (c.for_kind == ActionKind::button && !trigger && !from_end) {
        out.half = moved > 0 ? 1 : -1;
      }
      return CaptureResult::captured;
    }
  }
  return CaptureResult::waiting;
}

std::vector<std::string> conflicts(const ActionMap& map, std::string_view context,
                                   const Source& source) {
  std::vector<std::string> out;
  const Context* c = map.find_context(context);
  if (!c) return out;
  for (const Binding& b : c->bindings) {
    if (b.source == source && b.action < map.defs.size()) out.push_back(map.defs[b.action].name);
  }
  return out;
}

bool rebind(ActionMap& map, std::string_view context, std::string_view action, size_t slot,
            const Source& source, const std::string& device) {
  Context* c = map.find_context(context);
  const int a = map.find_action(action);
  if (!c || a < 0) return false;
  size_t seen = 0;
  for (Binding& b : c->bindings) {
    if (b.action != a) continue;
    if (seen++ == slot) {
      const Binding fresh = make_binding(b.action, source);
      b.shape = fresh.shape;
      b.source = source;
      b.device = device;
      b.with = {};
      return true;
    }
  }
  Binding b = make_binding(static_cast<uint16_t>(a), source);
  b.device = device;
  c->bindings.push_back(std::move(b));
  return true;
}

}  // namespace ge::input
