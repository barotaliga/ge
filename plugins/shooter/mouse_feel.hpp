// THE MOUSE'S FEEL: how a frame's raw counts become the turn, before `m_yaw` and `m_pitch`.
//
// - `m_filter` (0..1): the counts blended with the last frame's raw counts, the last frame
//   weighted half the setting (1: a plain average of the two frames; 0: off).
// - `m_accel` (0: off) with `m_accel_power` (2), `m_accel_offset` (0) and `m_accel_senscap` (0:
//   none): the faster the mouse moves, the higher the sensitivity. The speed is the frame's raw
//   counts' length over the frame's length in milliseconds less one (a millisecond at the least),
//   less the offset; above 0 it is times `m_accel`, raised to the power less 1 and added to
//   `sensitivity` (power 2: linear in the speed). A power of 1 or under, with the speed above the
//   offset, makes the sensitivity 1. The cap holds the result at most that high, slow moves too.
// - The sequel reads `m_accel` its own way (`per_frame`): the speed is the frame's counts' length
//   with no time, `m_accel` held within 0..1, and power 2, no offset, no cap.
// - `in_raw` (1): the counts as the device sends them; 0 the desktop's pointer speed and
//   acceleration applied first (the window asks it of the platform when it takes the mouse).
//
// A `sensitivity` under 0.5 (the window's old saves: degrees a count) is never accelerated.
//
// ge:summary The mouse's feel before the turn: m_filter, m_accel and its kin, in_raw
// ge:keywords mouse m_filter m_accel m_accel_power m_accel_offset m_accel_senscap in_raw
// ge:keywords mouse-acceleration mouse-smoothing raw-mouse feel_mouse sensitivity
// ge:skill input shooter
// ge:test tests/shooter_mouse_feel_test.cpp
#pragma once

#include "console/cvar.hpp"

namespace ge::shooter {

struct MouseFeel {
  double filter = 0;  // m_filter
  double accel = 0, accel_power = 2, accel_offset = 0, accel_senscap = 0;
  bool per_frame = false;  // the sequel's m_accel: counts a frame, not counts a millisecond
  bool raw = true;         // in_raw
  // The last frame's raw counts, for the filter.
  double last_dx = 0, last_dy = 0;
};

// One frame's counts shaped: the counts to turn by and the sensitivity to turn them at.
struct MouseTurn {
  double dx = 0, dy = 0;
  double sensitivity = 0;
};

// `m_filter`, `m_accel`, `m_accel_power`, `m_accel_offset`, `m_accel_senscap` and `in_raw` over
// `f` (`f` must outlive `v`).
void add_mouse_cvars(console::Cvars& v, MouseFeel& f);

// The frame's raw counts (`dx`, `dy`) over `dt` s shaped by `f` at `sensitivity`; the counts are
// kept for the next frame's filter.
MouseTurn feel_mouse(MouseFeel& f, double dx, double dy, double sensitivity, double dt);

}  // namespace ge::shooter
