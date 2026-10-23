// WHAT A DEVICE IS TOLD BACK: rumble, the triggers' rumble, a light's colour, the player number
// it shows, and FORCE FEEDBACK effects for a wheel or a stick. The game writes what it WANTS each
// device to do (`Feedback`, a desired state per device); a backend makes the device do it, and
// the simulated backend (sim.hpp) plays it on a simulated wheel so a lab can chart it.
//
// THE EFFECTS are the classic force-feedback set, on one axis (a wheel's turn, a stick's axis):
//   constant   a steady push of `magnitude` (-1 .. 1)
//   ramp       from `start` to `end` over the effect's duration
//   sine, square, triangle, saw_up, saw_down
//              periodic, `magnitude` about `offset` over `period_s` from `phase` (0..1 of a period)
//   spring     toward `center`: `coeff` per unit off it (beyond `deadband`), at most `saturation`
//   damper     against the speed: `coeff` per unit a second
//   friction   against the motion at any speed: `coeff`, smoothed across a standstill
//   inertia    against the acceleration: `coeff` per unit a second squared
// with an ENVELOPE (attack and fade levels and times), a duration (0: until stopped), a delay,
// and the device's gain and autocentre spring over them all. Forces are a share of the motor's
// full force (-1 .. 1, + toward the axis's +); `force_at` sums a device's effects at a moment,
// clamped to the motor's range: what the motor is driven with.
//
// Everything is a pure function of the times given (core::libm's sine): the same effect gives
// the same force on every machine, so a recorded session's feel is a digest.
//
// ge:summary Feedback: rumble, trigger rumble, light, player lights, force feedback effects
// ge:keywords input feedback rumble haptics vibration trigger-rumble light lightbar led player
// ge:keywords force-feedback ffb effect constant ramp sine square triangle sawtooth spring
// ge:keywords damper friction inertia envelope gain autocenter wheel motor force_at play_effect
// ge:skill input
// ge:test tests/input_feedback_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "input/device.hpp"
#include "input/state.hpp"

namespace ge::input {

enum class EffectKind : uint8_t {
  constant,
  ramp,
  sine,
  square,
  triangle,
  saw_up,
  saw_down,
  spring,
  damper,
  friction,
  inertia,
};

struct Envelope {
  float attack_s = 0, attack_level = 1;  // starts at attack_level, eases to 1 over attack_s
  float fade_s = 0, fade_level = 1;      // eases from 1 to fade_level over the last fade_s
};

struct Effect {
  EffectKind kind = EffectKind::constant;
  float magnitude = 0;  // a constant's push, a periodic's amplitude
  float offset = 0;     // a periodic's centre
  float period_s = 0.1f;
  float phase = 0;
  float start = 0, end = 0;  // a ramp's
  float center = 0;          // a spring's
  float coeff = 0;           // a condition's (spring, damper, friction, inertia)
  float saturation = 1;      // a condition's most force
  float deadband = 0;        // a spring's: no force within this of the centre
  float duration_s = 0;      // 0: until stopped
  float delay_s = 0;
  Envelope envelope;
};

inline constexpr int kMaxEffects = 8;

struct Playing {
  bool on = false;
  Effect effect;
  double started_s = 0;
};

struct DeviceFeedback {
  DeviceId id = 0;
  float low = 0, high = 0;    // the rumble motors, 0..1
  double rumble_until_s = 0;  // 0: until changed
  float left = 0, right = 0;  // the triggers' motors
  double triggers_until_s = 0;
  bool light_set = false;
  uint8_t r = 0, g = 0, b = 0;  // the light
  int player = -1;              // the player lights (-1: the device's own)
  float gain = 1;               // over every effect
  float autocenter = 0;         // a spring toward the centre, its coeff
  std::array<Playing, kMaxEffects> effects{};
};

struct Feedback {
  std::array<DeviceFeedback, kMaxDevices> d{};
  // The device's record, made on first use (null when every record is taken).
  DeviceFeedback* get(DeviceId id);
  const DeviceFeedback* find(DeviceId id) const;
  // A device unplugged: its record is dropped.
  void forget(DeviceId id);
};

void rumble(Feedback& f, DeviceId id, float low, float high, double duration_s, double now_s);
void rumble_triggers(Feedback& f, DeviceId id, float left, float right, double duration_s,
                     double now_s);
void set_light(Feedback& f, DeviceId id, uint8_t r, uint8_t g, uint8_t b);
// Start an effect; its handle (0 .. kMaxEffects-1), or -1 when every effect slot plays.
int play_effect(Feedback& f, DeviceId id, const Effect& e, double now_s);
void stop_effect(Feedback& f, DeviceId id, int handle);
// Change a playing effect's numbers without restarting it (a spring that stiffens with speed).
void update_effect(Feedback& f, DeviceId id, int handle, const Effect& e);

// The rumble as it stands at `now_s` (0 once its time is up).
void rumble_at(const DeviceFeedback& d, double now_s, float& low, float& high);
// One effect's force at `t_s` after it started, on an axis at `pos` (-1 .. 1) moving at `vel`
// (units/s) and accelerating at `acc` (units/s^2). Not clamped.
double effect_force(const Effect& e, double t_s, double pos, double vel, double acc);
// Every effect of the device, its gain and autocentre: the motor's drive, -1 .. 1.
double force_at(const DeviceFeedback& d, double now_s, double pos, double vel, double acc);
// Whether an effect is still playing at `now_s` (its duration not yet run).
bool playing(const Playing& p, double now_s);

}  // namespace ge::input
