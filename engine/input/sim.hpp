// THE SIMULATED BACKEND: devices that exist only in code, so everything above the platform (the
// state, the bindings, the players, rebinding, the feedback) runs and is measured in a container
// with no keyboard, no pad and no display. A test or a lab plugs in a keyboard, a mouse, a pad
// of any family, a wheel with pedals, a flight stick, a throttle or a touch screen, drives it
// (`sim_key`, `sim_axis`, `sim_touch`, ...) on a virtual clock, and ends each frame with
// `sim_frame`, which folds the frame's queue into an input state exactly as a real backend's
// frame is folded. The same frames written as an event log (event_log.hpp) replay to the same
// state, which is how a session with a real device is reproduced here too.
//
// THE SIMULATED WHEEL closes the force-feedback loop: a rim with inertia and its own friction,
// turned by a scripted hand (a torque, or a spring toward where the hand wants it) and by the
// motor, which is driven by the device's feedback (feedback.hpp's `force_at`). Its angle is the
// wheel's axis 0, sent as an event each frame, so a game's spring is felt as the wheel coming
// back to centre and a lab can chart it.
//
// ge:summary The simulated backend: virtual devices on a virtual clock, and a force-fed wheel
// ge:keywords input sim simulated virtual fake device backend headless test lab script clock
// ge:keywords sim_plug sim_key sim_axis sim_button sim_mouse sim_touch sim_frame sim_wheel
// ge:keywords force-feedback wheel-model inertia motor hand torque preset gamepad keyboard
// ge:skill input
// ge:test tests/input_sim_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/device.hpp"
#include "input/feedback.hpp"
#include "input/state.hpp"

namespace ge::input {

struct SimInput {
  uint64_t t_ns = 0;  // the virtual clock
  DeviceId next_id = 1;
  Queue queue;                      // the frame being written
  std::vector<DeviceInfo> devices;  // plugged in now
};

// A device as plugged in: its id (the info's id is set).
DeviceId sim_plug(SimInput& s, DeviceInfo info);
void sim_unplug(SimInput& s, DeviceId id);

// The presets: a device of each kind with the counts and features its kind has.
DeviceInfo sim_keyboard();
DeviceInfo sim_mouse();
DeviceInfo sim_pad(Family family = Family::xbox);
DeviceInfo sim_wheel();  // a wheel with three pedals on axes 1-3 and a shifter's buttons
DeviceInfo sim_flight_stick();
DeviceInfo sim_throttle();
DeviceInfo sim_pedals();
DeviceInfo sim_touch();

// Move the virtual clock on.
void sim_advance(SimInput& s, double dt_s);

void sim_key(SimInput& s, DeviceId kb, uint16_t usage, bool down);
void sim_text(SimInput& s, DeviceId kb, uint32_t code_point);
void sim_mouse_move(SimInput& s, DeviceId mouse, float dx, float dy);
void sim_mouse_button(SimInput& s, DeviceId mouse, MouseButton b, bool down);
void sim_wheel_scroll(SimInput& s, DeviceId mouse, float x, float y);
// A pad's button (PadButton) or a joystick's (its index), by the device's kind.
void sim_button(SimInput& s, DeviceId d, int button, bool down);
// A pad's axis (PadAxis) or a joystick's (its index).
void sim_axis(SimInput& s, DeviceId d, int axis, float value);
void sim_hat(SimInput& s, DeviceId d, int hat, uint8_t bits);
void sim_gyro(SimInput& s, DeviceId d, float x, float y, float z);
void sim_touch(SimInput& s, DeviceId screen, uint16_t finger, TouchPhase phase, float x, float y);
void sim_battery(SimInput& s, DeviceId d, float charge);

// End the frame: fold the queue into `state` and start the next frame's queue. The frame's
// queue is copied into `kept` first when given (to write an event log).
void sim_frame(SimInput& s, InputState& state, Queue* kept = nullptr);

// THE WHEEL'S RIM.
struct SimWheel {
  DeviceId id = 0;
  int axis = 0;
  double inertia = 0.04;             // the rim's, in motor-force units over (units/s^2): full force
                                     // turns it lock to lock (2 units) in about 0.28 s from rest
  double friction = 0.02;            // the rim's own, against the motion (force units)
  double hand = 0;                   // the hand's torque now (force units, + toward +)
  double hold = -1;                  // where the hand holds it (-1 .. 1), < -1: the hand is off
  double grip = 2;                   // how hard the hand pulls toward `hold` (force units per unit)
  double pos = 0, vel = 0, acc = 0;  // the rim's angle (-1 .. 1), speed and acceleration
  double motor = 0;                  // the motor's drive this step (-1 .. 1)
};
// Step the rim `dt_s` under the hand and the motor (the device's feedback at the clock's time)
// and send its angle as the wheel's axis.
void sim_wheel_step(SimInput& s, SimWheel& w, const Feedback& f, double dt_s);

}  // namespace ge::input
