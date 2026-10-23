// THE INPUT STATE: a frame's events folded into what is held, what went down and what came up,
// per device, so the bindings (actions.hpp) read a state and never an event. A backend writes a
// frame into a `Queue` (its events, and the `DeviceInfo` of every device plugged in during it);
// `update` clears the last frame's edges and deltas and applies the queue in order.
//
// THE EDGES COUNT TAPS. A key pressed and released inside one frame is `pressed` AND `released`
// in that frame and not `held` after it, so a tap shorter than a frame is never lost. A key's
// auto-repeat is not a press.
//
// EVERY DEVICE HAS A SLOT (its info, and its buttons and axes), found by its id; but every
// keyboard's keys are pooled into one keyboard and every mouse's motion into one mouse (a
// laptop's touch pad and a plugged mouse move one pointer), and every touch screen's fingers
// into one screen. The mouse's
// motion is SUMMED over the frame in doubles, so a high-rate mouse's many small raw deltas lose
// nothing. A device unplugged keeps nothing: its slot is freed, and anything held on it is
// released (its `released` edges are set in that frame).
//
// THE LAST USED DEVICE is tracked for prompts: the device of the last press, the last axis moved
// past half way, the last real mouse motion. A HUD prints its glyphs (names.hpp) off it.
//
// Nothing here allocates after `reserve` (the queue) and the state is a fixed block.
//
// ge:summary The input state: a frame's events into held, pressed and released per device
// ge:keywords input state held pressed released edges tap queue update apply frame keyboard
// ge:keywords mouse delta wheel slot device-slot touch finger text last-used plug unplug
// ge:keywords input-state input-queue key_held key_pressed find_slot
// ge:skill input
// ge:test tests/input_state_test.cpp
#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

#include "input/device.hpp"
#include "input/event.hpp"

namespace ge::input {

inline constexpr int kKeys = 512;         // HID usages (keys.hpp)
inline constexpr int kMaxDevices = 16;    // plugged in at once, of every kind
inline constexpr int kMaxFingers = 10;    // on the screen
inline constexpr int kMaxPadFingers = 4;  // on a pad's touch pad
inline constexpr int kMaxText = 64;       // code points typed in one frame

// One frame of a backend's output: the events in order, and the info of every device plugged
// in during it (a `device_added` event's `code` indexes `plugged`).
struct Queue {
  std::vector<Event> events;
  std::vector<DeviceInfo> plugged;

  void reserve(size_t events_n) { events.reserve(events_n); }
  void clear() {
    events.clear();
    plugged.clear();
  }
  void push(const Event& e) { events.push_back(e); }
  void plug(uint64_t t_ns, const DeviceInfo& info);
  void unplug(uint64_t t_ns, DeviceId id);
};

struct Edges64 {
  uint64_t held = 0, pressed = 0, released = 0;
  bool down(int i) const { return (held >> i) & 1u; }
  bool went_down(int i) const { return (pressed >> i) & 1u; }
  bool went_up(int i) const { return (released >> i) & 1u; }
};

struct Finger {
  bool active = false;  // on the glass after this frame's events
  bool began = false;   // touched down this frame
  bool ended = false;   // lifted this frame
  uint16_t id = 0;
  float x = 0, y = 0;    // 0..1 of the surface, from the top left
  float x0 = 0, y0 = 0;  // where it touched down
  float force = 0;
};

// One device. A keyboard's, a mouse's and a touch screen's input is pooled (above); the slot
// holds their info.
struct Slot {
  bool active = false;
  DeviceInfo info;
  Edges64 buttons;  // a gamepad's PadButton bits, or a joystick's buttons by index
  std::array<float, kMaxAxes> axes{};  // a gamepad's PadAxis, or a joystick's axes by index
  std::array<uint8_t, kMaxHats> hats{};
  std::array<uint8_t, kMaxHats> hats_pressed{};  // Hat bits that went down this frame
  std::array<float, 3> gyro{};                   // rad/s
  std::array<float, 3> accel{};                  // m/s^2
  std::array<Finger, kMaxPadFingers> pad_fingers{};
  float battery = -1;  // 0..1, -1 unknown
};

struct Mouse {
  double dx = 0, dy = 0;           // this frame's motion, summed (raw counts when `raw`)
  bool raw = false;                // the last motion was the device's raw counts
  float x = 0, y = 0;              // the pointer, from the top left (the backend's units)
  float wheel_x = 0, wheel_y = 0;  // this frame's wheel, summed (+y away from the player)
  Edges64 buttons;                 // MouseButton bits
};

struct InputState {
  std::bitset<kKeys> keys_held, keys_pressed, keys_released;
  Mouse mouse;
  std::array<Slot, kMaxDevices> slots{};
  std::array<Finger, kMaxFingers> fingers{};
  std::array<uint32_t, kMaxText> text{};
  int text_n = 0;
  DeviceId keyboard = 0, mouse_id = 0, touch_id = 0;  // the last of each kind seen (0 none)
  DeviceId last_used = 0;
  DeviceKind last_kind = DeviceKind::none;
  // The devices that came and went this frame.
  std::array<DeviceId, kMaxDevices> added{}, removed{};
  int added_n = 0, removed_n = 0;
  uint64_t t_ns = 0;  // the last event's time
};

// The slot of a device, or null.
Slot* find_slot(InputState& s, DeviceId id);
const Slot* find_slot(const InputState& s, DeviceId id);

// Clear the last frame's edges, deltas, text and comings and goings.
void begin_frame(InputState& s);
// Apply one event (`plugged` is the queue's info table, read by `device_added`).
void apply(InputState& s, const Event& e, const std::vector<DeviceInfo>& plugged);
// begin_frame, then apply every event of the queue in order.
void update(InputState& s, const Queue& q);

inline bool key_held(const InputState& s, uint16_t usage) {
  return usage < kKeys && s.keys_held[usage];
}
inline bool key_pressed(const InputState& s, uint16_t usage) {
  return usage < kKeys && s.keys_pressed[usage];
}

// How many devices of `kind` are plugged in (keyboards and mice count as one each when seen).
int count_kind(const InputState& s, DeviceKind kind);

}  // namespace ge::input
