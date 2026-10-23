// THE OS'S INPUT DEVICES: the platform's backend of engine/input. It turns what SDL3 reports
// (keyboards, mice, gamepads, joysticks, wheels, flight sticks, throttles, touch screens; their
// plugging in and out) into `ge::input` events and device descriptions on an `input::Queue`, and
// plays `input::Feedback` (rumble, trigger rumble, the light, the player lights, force feedback)
// back on the hardware. Everything above it sees only engine/input's types: a game binds actions
// to the same sources whether the events came from here, the simulated backend or a log.
//
// WHAT COMES THROUGH, and how:
//   keyboard   one pooled keyboard device; keys by USB HID usage (SDL's scancodes are the same
//              numbers), repeats flagged; text as code points while text input is on.
//   mouse      one pooled mouse; with relative mode on (`set_relative_mouse`), the motion is the
//              device's own counts with no pointer acceleration (aux = 1), which a first-person
//              look needs; buttons 1..5, the wheel with its sub-notch fractions.
//   gamepad    anything SDL maps to a standard pad: positional buttons and axes in engine/input's
//              numbering (which is SDL's), the face-button family from its type, the gyro and the
//              accelerometer when present (`DeviceOptions::sensors`), the touchpad's fingers, the
//              battery.
//   joystick   everything else with axes: its kind from SDL's joystick type (wheel, flight stick,
//              throttle; the rest are `joystick`), raw axes -1..1, buttons and hats; force feedback
//              when the device has a haptic interface.
//   touch      a touch screen's fingers, 0..1 of the screen; the device is plugged the first time
//              it reports a finger.
// Device ids are this backend's own, never reused in a session; a device's `key` is its USB ids
// ("vvvv:pppp", what the device database is keyed on) or, without them, SDL's GUID.
//
// FORCE FEEDBACK is computed by engine/input as a pure function (`input::force_at`: the effects,
// the gain, the autocenter, from the wheel's position) and played here as ONE constant force the
// backend updates every frame, so a wheel feels the same on every OS and in a lab with no wheel at
// all. The device's own autocenter is switched off: input::Feedback's autocenter replaces it.
//
// VIRTUAL DEVICES: `attach_virtual` plugs a device SDL treats as real hardware (SDL's virtual
// joystick driver), so the whole path (SDL's mapping, its events, this translation, rumble going
// back out) runs headless in a test or a lab with no device and no display.
//
// THREADING: every call on the main thread, once per frame. With a window, `poll_input` (the
// overload below) reads the window's events and the devices' in one pump; with none (a test, a
// lab, a headless tool), `pump_devices` does.
//
// ge:summary The OS's input devices over SDL3 as engine/input events; rumble and force played back
// ge:keywords platform input-backend sdl gamepad joystick wheel flight-stick throttle hotas
// ge:keywords keyboard mouse raw-mouse relative-mouse text-input touch hot-plug rumble haptic
// ge:keywords force-feedback ffb lightbar player-lights gyro accelerometer touchpad battery
// ge:keywords virtual-joystick attach_virtual pump_devices play_feedback open_devices
// ge:skill input platform
// ge:test tests/platform_devices_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "input/device_db.hpp"
#include "input/feedback.hpp"
#include "input/state.hpp"
#include "platform/window.hpp"

namespace ge::platform {

struct DeviceOptions {
  bool sensors = true;   // a gamepad's gyro and accelerometer (they cost the pad battery)
  bool haptics = true;   // force feedback on joysticks and wheels that have it
  bool keyboard = true;  // plug the pooled keyboard and mouse (off: pads and sticks alone)
};

class Devices;
struct DevicesDeleter {
  void operator()(Devices* d) const;
};
using DevicesPtr = std::unique_ptr<Devices, DevicesDeleter>;

// Start the devices' subsystems (SDL's gamepad, joystick, haptic and sensor). Every device
// already plugged in arrives as a device_added on the first pump. Returns nullptr after an
// agent-actionable error line when the subsystems cannot start.
DevicesPtr open_devices(const DeviceOptions& options = {});

// Read the devices' pending events into `queue` (with no window: a window's `poll_input` below
// reads them in the same pump). Appends; the caller clears the queue each frame.
void pump_devices(Devices& devices, input::Queue& queue);

// The window's events into `snapshot` and every device's into `queue`, in one pump.
void poll_input(Window& window, InputSnapshot& snapshot, Devices& devices, input::Queue& queue);

// Play back what engine/input asks of each device this frame: the rumble motors and the
// triggers' (resent only when they change, held between frames), the light, the player lights,
// and each force-feedback device's force from its effects at its steering axis (`db` gives a
// wheel's axis; without a row, axis 0). `now_s` is the same clock the effects were started on.
void play_feedback(Devices& devices, const input::Feedback& feedback,
                   const input::InputState& state, double now_s,
                   const input::DeviceDb* db = nullptr);

// The pointer's motion as raw counts with the pointer hidden and held (a first-person look), or
// the pointer back. Off by default.
void set_relative_mouse(Window& window, bool on);
// Text events (a console, a name field; on a phone, the on-screen keyboard). Off by default.
void set_text_input(Window& window, bool on);

// A force (-1 .. 1, input::force_at's) as the level a constant haptic effect is given: clamped,
// and 0 inside one count so a resting wheel's motor is silent.
int16_t haptic_level(double force);

// --- Virtual devices (SDL's virtual joystick driver): real to SDL, made by a test or a lab ---

struct VirtualDesc {
  input::DeviceKind kind = input::DeviceKind::gamepad;  // gamepad, joystick, wheel, flight_stick,
                                                        // throttle
  std::string name = "virtual device";
  uint16_t vendor = 0, product = 0;
  int axes = 6, buttons = 21, hats = 0;  // a gamepad's are its standard set
  bool gyro = false;                     // a gamepad's gyro and accelerometer
  int touchpads = 0;                     // a gamepad's touchpad (one finger each)
};

// What a virtual device was last told by play_feedback (through SDL, as hardware would be).
struct VirtualFeedback {
  uint16_t low = 0, high = 0;    // the rumble motors, SDL's 0 .. 65535
  uint16_t left = 0, right = 0;  // the triggers'
  uint8_t r = 0, g = 0, b = 0;   // the light
  bool light = false;            // the light has been set
  int player = -1;               // the player index set
  int rumbles = 0;               // rumble calls that reached the device
};

// Plug a virtual device; returns its handle (not the engine/input DeviceId, which arrives with
// its device_added on the next pump), or 0 after an error line.
uint32_t attach_virtual(Devices& devices, const VirtualDesc& desc);
// Unplug it (a device_removed on the next pump).
void detach_virtual(Devices& devices, uint32_t handle);
// Set its state; it is reported on the next pump. A gamepad's axes are engine/input's (sticks
// -1..1, triggers 0..1) and its buttons PadButton; a joystick's axes -1..1.
void virtual_axis(Devices& devices, uint32_t handle, int axis, float value);
void virtual_button(Devices& devices, uint32_t handle, int button, bool down);
void virtual_hat(Devices& devices, uint32_t handle, int hat, uint8_t bits);
void virtual_sensor(Devices& devices, uint32_t handle, bool gyro, float x, float y, float z);
void virtual_touch(Devices& devices, uint32_t handle, int touchpad, bool down, float x, float y);
VirtualFeedback virtual_feedback(const Devices& devices, uint32_t handle);
// A key or a mouse's motion as if typed or moved on the window, through SDL's own event queue
// (so the window's pump, the devices' translation and the raw flag of a relative mouse are the
// real path); read on the next pump.
void virtual_key(Devices& devices, uint16_t usage, bool down);
void virtual_mouse_motion(Devices& devices, float dx, float dy);
// The engine/input id the virtual device was plugged as (0 until its device_added was pumped).
input::DeviceId virtual_id(const Devices& devices, uint32_t handle);

}  // namespace ge::platform
