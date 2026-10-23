// THE INPUT EVENT: one thing a device did, as every backend reports it (the platform's, the
// simulated one, a log played back). It is a 32-byte POD so a frame's events are one flat
// array: the time it happened on the backend's monotonic clock, the device, what kind of thing
// happened, a code (which key, button, axis or finger) and up to four values. The table below
// is the whole vocabulary; `state.hpp` folds a frame's events into the state the bindings
// read, and `event_log.hpp` writes them as text.
//
//   type           code                 aux             v[0..3]
//   device_added   -                    -               - (the DeviceInfo is the backend's)
//   device_removed -                    -               -
//   key            HID usage (keys.hpp) 1 = a repeat    v0 1 down, 0 up
//   text           -                    -               v0 the Unicode code point typed (a console,
//   a name) mouse_motion   -                    1 = raw counts  v0 dx, v1 dy (right, down), v2 x,
//   v3 y mouse_button   0 left 1 middle 2 right 3 back 4 forward      v0 1 down, 0 up mouse_wheel
//   -                    -               v0 x (right), v1 y (away from the player) pad_button
//   PadButton            -               v0 1 down, 0 up pad_axis       PadAxis              - v0
//   the value (sticks -1..1, triggers 0..1) joy_button     index                -               v0
//   1 down, 0 up joy_axis       index                -               v0 the value, -1 .. 1 joy_hat
//   index                -               v0 the Hat bits now held sensor         0 gyro, 1 accel -
//   v0..v2 x y z (rad/s; m/s^2) touch          finger id            TouchPhase      v0 x, v1 y
//   (0..1 of the screen), v2 force pad_touch      finger id            TouchPhase      v0 x, v1 y
//   (0..1 of the pad), v2 force, v3 pad battery        -                    -               v0 the
//   charge 0..1 (-1 unknown)
//
// The mouse's motion is the RAW delta when the backend can give it (aux = 1: the device's own
// counts, no pointer acceleration, sub-pixel), which is what a first-person look needs; the
// position is the pointer's, for a menu.
//
// ge:summary The input event: one 32-byte POD for every device action, and its type table
// ge:keywords input event events key keyboard mouse motion raw-mouse wheel button axis hat
// ge:keywords sensor gyro accelerometer touch finger touchpad battery text device-added
// ge:keywords device-removed hot-plug event-type touch-phase
// ge:skill input
// ge:test tests/input_state_test.cpp
#pragma once

#include <cstdint>

#include "input/device.hpp"

namespace ge::input {

enum class EventType : uint8_t {
  device_added,
  device_removed,
  key,
  text,
  mouse_motion,
  mouse_button,
  mouse_wheel,
  pad_button,
  pad_axis,
  joy_button,
  joy_axis,
  joy_hat,
  sensor,
  touch,
  pad_touch,
  battery,
};
inline constexpr int kEventTypes = 16;

enum class TouchPhase : uint8_t { down, move, up };

enum class MouseButton : uint8_t { left, middle, right, back, forward };
inline constexpr int kMouseButtons = 5;

struct Event {
  uint64_t t_ns = 0;  // the backend's monotonic clock
  DeviceId device = 0;
  EventType type = EventType::key;
  uint8_t aux = 0;
  uint16_t code = 0;
  float v[4] = {0, 0, 0, 0};
};
static_assert(sizeof(Event) == 32, "an event is 32 bytes: the log and the queue rely on it");

// Builders, so a backend or a script states only what matters.
inline Event key_event(uint64_t t, DeviceId d, uint16_t usage, bool down, bool repeat = false) {
  return {t, d, EventType::key, static_cast<uint8_t>(repeat), usage, {down ? 1.0f : 0.0f}};
}
inline Event button_event(uint64_t t, DeviceId d, EventType type, uint16_t code, bool down) {
  return {t, d, type, 0, code, {down ? 1.0f : 0.0f}};
}
inline Event axis_event(uint64_t t, DeviceId d, EventType type, uint16_t code, float value) {
  return {t, d, type, 0, code, {value}};
}
inline Event motion_event(uint64_t t, DeviceId d, float dx, float dy, float x, float y, bool raw) {
  return {t, d, EventType::mouse_motion, static_cast<uint8_t>(raw), 0, {dx, dy, x, y}};
}
inline Event touch_event(uint64_t t, DeviceId d, EventType type, uint16_t finger, TouchPhase phase,
                         float x, float y, float force = 1, float pad = 0) {
  return {t, d, type, static_cast<uint8_t>(phase), finger, {x, y, force, pad}};
}

}  // namespace ge::input
