// THE DEVICE MODEL: what a player holds, described the same whatever the platform reports it
// through. A device is one entry in a table the backend fills (`DeviceInfo`): its kind (a
// keyboard, a mouse, a gamepad, a plain joystick, a wheel, a flight stick, a throttle, pedals, a
// touch screen), its family (how its face buttons are labelled), its counts of buttons, axes and
// hats, and what it can be told back (`Feature`: rumble, trigger rumble, a light, gyro, a touch
// pad, force feedback). Everything above the backend (the state, the bindings, the players)
// reads this table and never asks the platform.
//
// THE CONTROL CODES are positional and stated here once. A gamepad's buttons are named by where
// they sit (`south` is the bottom face button, whatever it is printed with), so one binding
// works on every pad; `names.hpp` gives the label each family prints on it. A gamepad's axes
// are its two sticks (-1 .. 1, +x right, +y DOWN as the stick is pushed toward the player) and
// its two triggers (0 .. 1). A device that is not a gamepad reports raw joystick axes, buttons
// and hats by index; a wheel's or a throttle's axes get their meaning (steer, clutch, the
// throttle) from its row in the device database (device_db.hpp).
//
// ge:summary The device model: kinds, families, features, gamepad buttons and axes, DeviceInfo
// ge:keywords input device kind family feature gamepad joystick wheel pedals throttle hotas
// ge:keywords flight-stick keyboard mouse touch controller rumble gyro touchpad led battery
// ge:keywords gamepad-button gamepad-axis hat device-info vendor product guid hot-plug
// ge:skill input
// ge:test tests/input_state_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace ge::input {

// A device's id for as long as it stays plugged in. 0 is no device. The backend never reuses
// an id within a session, so an event for a removed device can never reach a new one.
using DeviceId = uint32_t;

enum class DeviceKind : uint8_t {
  none,
  keyboard,
  mouse,
  gamepad,       // a standard pad: two sticks, two triggers, the face buttons, the d-pad
  joystick,      // anything with axes and buttons the platform does not map to a gamepad
  wheel,         // a steering wheel (its pedals and shifter may be on it or a device of their own)
  flight_stick,  // a stick, often with a twist rudder and a hat
  throttle,      // a throttle quadrant (a HOTAS's left hand)
  pedals,        // rudder or car pedals as a device of their own
  touch,         // a touch screen
};
inline constexpr int kDeviceKinds = 10;

// How a gamepad's face buttons are labelled (names.hpp), so a prompt can print what the player
// sees. The binding is positional and never depends on it.
enum class Family : uint8_t {
  generic,      // letters by position: A B X Y with A at the bottom
  xbox,         // A (south), B (east), X (west), Y (north)
  playstation,  // cross, circle, square, triangle
  nintendo,     // B (south), A (east), Y (west), X (north)
};

// What a device can be told back or read beyond its buttons and axes; a bitmask.
enum Feature : uint16_t {
  kRumble = 1u << 0,         // two motors, low and high frequency
  kTriggerRumble = 1u << 1,  // a motor in each trigger
  kLight = 1u << 2,          // a colour light (a lightbar)
  kPlayerLights = 1u << 3,   // the player-number lights
  kGyro = 1u << 4,           // angular rate, rad/s
  kAccel = 1u << 5,          // acceleration, m/s^2
  kTouchpad = 1u << 6,       // a touch surface on the pad
  kForce = 1u << 7,          // force feedback effects (feedback.hpp): wheels, sticks
  kBattery = 1u << 8,        // reports its charge
  kRawMotion = 1u << 9,      // a mouse whose motion arrives unaccelerated (raw counts)
};

// A gamepad's buttons, by position. The numbering is part of the event log's format.
enum class PadButton : uint8_t {
  south,
  east,
  west,
  north,
  back,
  guide,
  start,
  left_stick,
  right_stick,
  left_shoulder,
  right_shoulder,
  dpad_up,
  dpad_down,
  dpad_left,
  dpad_right,
  misc,  // a share or capture or mute button
  right_paddle1,
  left_paddle1,
  right_paddle2,
  left_paddle2,
  touchpad,  // the touch pad clicked
};
inline constexpr int kPadButtons = 21;

// A gamepad's axes. Sticks -1 .. 1 (+x right, +y toward the player); triggers 0 .. 1.
enum class PadAxis : uint8_t { left_x, left_y, right_x, right_y, left_trigger, right_trigger };
inline constexpr int kPadAxes = 6;

// A hat's directions as a bitmask (a diagonal sets two).
enum Hat : uint8_t { kHatUp = 1, kHatRight = 2, kHatDown = 4, kHatLeft = 8 };

// The most of each a device's state keeps; a device that reports more keeps the first ones
// and the backend notes the rest once.
inline constexpr int kMaxButtons = 64;
inline constexpr int kMaxAxes = 16;
inline constexpr int kMaxHats = 4;

// One device as the backend describes it when it is plugged in.
struct DeviceInfo {
  DeviceId id = 0;
  DeviceKind kind = DeviceKind::none;
  Family family = Family::generic;
  std::string name;                  // as the device reports it
  uint16_t vendor = 0, product = 0;  // USB ids (0 when the transport has none)
  // A stable key for this model of device across sessions (the platform's GUID, or
  // "vvvv:pppp" from the USB ids): what a saved binding or a database row is keyed on.
  std::string key;
  uint32_t features = 0;                // Feature bits
  int buttons = 0, axes = 0, hats = 0;  // a joystick's raw counts (a gamepad's are fixed)
  int touchpads = 0;
  int player = -1;  // the player number the device itself shows, when it has lights for it
};

inline bool has(const DeviceInfo& d, Feature f) { return (d.features & f) != 0; }

// The stable key from USB ids, "vvvv:pppp" in lower-case hex.
std::string usb_key(uint16_t vendor, uint16_t product);

}  // namespace ge::input
