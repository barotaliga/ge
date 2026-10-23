// THE DEVICE DATABASE: what a wheel's, a flight stick's, a throttle's or a set of pedals' raw
// axes and buttons MEAN. The platform maps a gamepad to the standard layout (device.hpp), but a
// wheel reports "axis 2" and nothing says whether that is the clutch, which end is at rest or
// how far the wheel turns. A row, keyed by the device's model (`DeviceInfo::key`), gives each
// raw axis a ROLE and its range, and names the buttons that are gears and paddles; a binding
// then says `role:clutch` and works on every wheel that has a row.
//
// THE RANGE: an axis role reads `lo` .. `hi` of the raw axis (-1 .. 1). A signed role (the
// steer, a stick's axes, a twist, the rudder) maps it onto -1 .. 1; a lever role (a pedal, a
// throttle, a slider) onto 0 .. 1, so a pedal that rests at +1 and reads -1 floored is
// `lo = 1, hi = -1`.
//
// LEARNING A ROW: a player who plugs in a device with no row teaches it (`Learn`): for each role
// in turn, move the control through its whole travel and let go; the raw axis that travelled
// furthest is the role's, its rest says which end is `lo`. A press while learning a button role
// names its button. The result is saved in the same text the shipped rows are (`write_db`).
//
// DEFAULTS when a device has no row: a wheel's axis 0 is the steer, a flight stick's axes 0 and
// 1 its roll and pitch; nothing else is guessed.
//
//   ge-devices 1
//   device 046d:c24f wheel degrees=900 "a wheel"
//   axis steer 0 -1 1
//   axis brake 3 1 -1
//   button shift_up 4
//
// ge:summary The device database: roles and ranges for wheels, pedals, sticks, throttles; learning
// ge:keywords input device-db database wheel pedals shifter gear paddle clutch throttle brake
// ge:keywords handbrake flight-stick hotas twist rudder toe-brake slider role range calibrate
// ge:keywords learn calibration degrees lock-to-lock role_value read_db write_db device-row
// ge:skill input
// ge:test tests/input_device_db_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "input/state.hpp"

namespace ge::input {

enum class Role : uint8_t {
  none,
  // Signed axes, -1 .. 1.
  steer,
  stick_x,  // a flight stick's roll (right +)
  stick_y,  // its pitch (pulled back +)
  twist,    // a stick's twist (right +)
  rudder,   // rudder pedals (right +)
  mini_x,   // a throttle's thumb stick
  mini_y,
  // Lever axes, 0 .. 1.
  throttle,
  brake,
  clutch,
  handbrake,
  toe_left,  // toe brakes on rudder pedals
  toe_right,
  slider,
  // Buttons.
  shift_up,
  shift_down,
  gear_1,
  gear_2,
  gear_3,
  gear_4,
  gear_5,
  gear_6,
  gear_7,
  gear_reverse,
};
inline constexpr int kRoles = 25;

bool is_signed(Role r);
bool is_button(Role r);
std::string_view role_name(Role r);
std::optional<Role> role_from_name(std::string_view name);

struct AxisRole {
  Role role = Role::none;
  uint8_t axis = 0;
  float lo = -1, hi = 1;  // the raw axis's reading at the role's 0 (or -1) and at its 1
};

struct ButtonRole {
  Role role = Role::none;
  uint8_t button = 0;
};

struct DeviceRow {
  std::string key;  // DeviceInfo::key
  DeviceKind kind = DeviceKind::joystick;
  std::string name;
  float degrees = 0;  // a wheel's lock to lock (0 unknown)
  std::vector<AxisRole> axes;
  std::vector<ButtonRole> buttons;
};

struct DeviceDb {
  std::vector<DeviceRow> rows;
  const DeviceRow* find(std::string_view key) const;
  // Add the row, or replace the one with its key.
  void put(DeviceRow row);
};

// The row a device is read by: its own, or its kind's default (empty roles otherwise).
DeviceRow row_for(const DeviceDb* db, const DeviceInfo& info);
// A role's value on a slot: the axis through its range (0 when the row has no such role), or a
// button role 0 or 1. `found` says whether the device has the role at all.
float role_value(const DeviceRow& row, const Slot& slot, Role role, bool* found = nullptr);

std::string write_db(const DeviceDb& db);
bool read_db(std::string_view text, DeviceDb& db, std::string& error);

// TEACHING A ROW, one role at a time.
struct Learn {
  Role role = Role::none;
  bool active = false;
  std::array<float, kMaxAxes> rest{}, lo{}, hi{};
  uint64_t pressed = 0;  // buttons that went down while learning
};
// Start learning `role` on the slot as it stands now (its rest).
void learn_begin(Learn& l, const Slot& slot, Role role);
// Watch one frame.
void learn_feed(Learn& l, const Slot& slot);
// The axis role the travel says, when an axis travelled more than half its range; or the
// button role, for a button role, when one was pressed.
std::optional<AxisRole> learn_axis(const Learn& l);
std::optional<ButtonRole> learn_button(const Learn& l);

}  // namespace ge::input
