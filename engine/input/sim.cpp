// The simulated backend's devices, presets, drivers and the wheel's rim.
//
// ge:summary The simulated devices' presets and drivers, the frame, the wheel rim's step
// ge:keywords input sim presets drivers wheel rim step
// ge:skill input
// ge:test tests/input_sim_test.cpp
#include "input/sim.hpp"

#include <algorithm>
#include <cmath>

namespace ge::input {

namespace {

DeviceKind kind_of(const SimInput& s, DeviceId id) {
  for (const DeviceInfo& d : s.devices) {
    if (d.id == id) return d.kind;
  }
  return DeviceKind::none;
}

DeviceInfo preset(DeviceKind kind, const char* name, uint16_t vendor, uint16_t product,
                  uint32_t features, int axes, int buttons, int hats) {
  DeviceInfo d;
  d.kind = kind;
  d.name = name;
  d.vendor = vendor;
  d.product = product;
  d.key = usb_key(vendor, product);
  d.features = features;
  d.axes = axes;
  d.buttons = buttons;
  d.hats = hats;
  return d;
}

}  // namespace

// The simulated devices' USB ids are in the range USB reserves for no vendor (0xfff0 up), so a
// simulated row can never be mistaken for a real device's.
DeviceInfo sim_keyboard() {
  return preset(DeviceKind::keyboard, "simulated keyboard", 0xfff0, 1, 0, 0, 0, 0);
}
DeviceInfo sim_mouse() {
  return preset(DeviceKind::mouse, "simulated mouse", 0xfff0, 2, kRawMotion, 0, 5, 0);
}
DeviceInfo sim_pad(Family family) {
  DeviceInfo d = preset(
      DeviceKind::gamepad, "simulated pad", 0xfff1,
      static_cast<uint16_t>(1 + static_cast<int>(family)),
      kRumble | kTriggerRumble | kLight | kPlayerLights | kGyro | kAccel | kTouchpad | kBattery,
      kPadAxes, kPadButtons, 0);
  d.family = family;
  d.touchpads = 1;
  return d;
}
DeviceInfo sim_wheel() {
  return preset(DeviceKind::wheel, "simulated wheel", 0xfff2, 1, kForce, 4, 24, 1);
}
DeviceInfo sim_flight_stick() {
  return preset(DeviceKind::flight_stick, "simulated flight stick", 0xfff2, 2, kForce, 3, 16, 1);
}
DeviceInfo sim_throttle() {
  return preset(DeviceKind::throttle, "simulated throttle", 0xfff2, 3, 0, 4, 20, 1);
}
DeviceInfo sim_pedals() {
  return preset(DeviceKind::pedals, "simulated pedals", 0xfff2, 4, 0, 3, 0, 0);
}
DeviceInfo sim_touch() {
  return preset(DeviceKind::touch, "simulated touch screen", 0xfff3, 1, 0, 0, 0, 0);
}

DeviceId sim_plug(SimInput& s, DeviceInfo info) {
  info.id = s.next_id++;
  s.devices.push_back(info);
  s.queue.plug(s.t_ns, info);
  return info.id;
}

void sim_unplug(SimInput& s, DeviceId id) {
  s.devices.erase(std::remove_if(s.devices.begin(), s.devices.end(),
                                 [id](const DeviceInfo& d) { return d.id == id; }),
                  s.devices.end());
  s.queue.unplug(s.t_ns, id);
}

void sim_advance(SimInput& s, double dt_s) {
  s.t_ns += static_cast<uint64_t>(std::llround(dt_s * 1e9));
}

void sim_key(SimInput& s, DeviceId kb, uint16_t usage, bool down) {
  s.queue.push(key_event(s.t_ns, kb, usage, down));
}

void sim_text(SimInput& s, DeviceId kb, uint32_t code_point) {
  s.queue.push({s.t_ns, kb, EventType::text, 0, 0, {static_cast<float>(code_point)}});
}

void sim_mouse_move(SimInput& s, DeviceId mouse, float dx, float dy) {
  s.queue.push(motion_event(s.t_ns, mouse, dx, dy, 0, 0, true));
}

void sim_mouse_button(SimInput& s, DeviceId mouse, MouseButton b, bool down) {
  s.queue.push(
      button_event(s.t_ns, mouse, EventType::mouse_button, static_cast<uint16_t>(b), down));
}

void sim_wheel_scroll(SimInput& s, DeviceId mouse, float x, float y) {
  s.queue.push({s.t_ns, mouse, EventType::mouse_wheel, 0, 0, {x, y}});
}

void sim_button(SimInput& s, DeviceId d, int button, bool down) {
  const EventType t =
      kind_of(s, d) == DeviceKind::gamepad ? EventType::pad_button : EventType::joy_button;
  s.queue.push(button_event(s.t_ns, d, t, static_cast<uint16_t>(button), down));
}

void sim_axis(SimInput& s, DeviceId d, int axis, float value) {
  const EventType t =
      kind_of(s, d) == DeviceKind::gamepad ? EventType::pad_axis : EventType::joy_axis;
  s.queue.push(axis_event(s.t_ns, d, t, static_cast<uint16_t>(axis), value));
}

void sim_hat(SimInput& s, DeviceId d, int hat, uint8_t bits) {
  s.queue.push(axis_event(s.t_ns, d, EventType::joy_hat, static_cast<uint16_t>(hat), bits));
}

void sim_gyro(SimInput& s, DeviceId d, float x, float y, float z) {
  s.queue.push({s.t_ns, d, EventType::sensor, 0, 0, {x, y, z}});
}

void sim_touch(SimInput& s, DeviceId screen, uint16_t finger, TouchPhase phase, float x, float y) {
  s.queue.push(touch_event(s.t_ns, screen, EventType::touch, finger, phase, x, y));
}

void sim_battery(SimInput& s, DeviceId d, float charge) {
  s.queue.push(axis_event(s.t_ns, d, EventType::battery, 0, charge));
}

void sim_frame(SimInput& s, InputState& state, Queue* kept) {
  if (kept) *kept = s.queue;
  update(state, s.queue);
  s.queue.clear();
}

void sim_wheel_step(SimInput& s, SimWheel& w, const Feedback& f, double dt_s) {
  const DeviceFeedback* d = f.find(w.id);
  const double now = static_cast<double>(s.t_ns) * 1e-9;
  w.motor = d ? force_at(*d, now, w.pos, w.vel, w.acc) : 0;
  double torque = w.motor + w.hand;
  if (w.hold >= -1) torque += w.grip * (w.hold - w.pos) - 0.1 * w.grip * w.vel;
  // The rim's friction stops a slow rim rather than reversing it.
  const double fr = w.vel > 0 ? -w.friction : (w.vel < 0 ? w.friction : 0);
  double acc = (torque + fr) / w.inertia;
  if (w.vel == 0 && (torque < w.friction && torque > -w.friction)) acc = 0;
  double vel = w.vel + acc * dt_s;
  if ((w.vel > 0 && vel < 0 && torque <= 0) || (w.vel < 0 && vel > 0 && torque >= 0)) vel = 0;
  double pos = w.pos + vel * dt_s;
  if (pos > 1 || pos < -1) {  // the lock's stop
    pos = pos > 1 ? 1 : -1;
    vel = 0;
  }
  w.acc = (vel - w.vel) / dt_s;
  w.vel = vel;
  w.pos = pos;
  sim_axis(s, w.id, w.axis, static_cast<float>(pos));
}

}  // namespace ge::input
