// WHAT A RUN WAS SKIED ON, TO THE FIGURE: Fall Line's control tape schema (the racing plugin's
// tape, `racing/tape.hpp`, carries it) and `snap_controls`, which puts every axis on the tape's
// grid. Every pilot's controls are snapped before `step` is handed them, so the figure the run
// is skied on IS the figure the tape writes down, and a tape played back is the run again.
//
// The axes, in the stored tape's order: `steer` (signed), `tuck` (lever), `brake` (lever),
// `flags` (bit 0 the jump held, bit 1 the reset). Renaming or reordering one is a format change.
//
// ge:summary The control tape's schema and snap_controls: the run's controls on the tape's grid
// ge:keywords controls tape schema snap snap_controls ghost replay steer tuck brake jump
// ge:keywords reset flags controls_to_axes controls_from_axes
// ge:skill sim racing
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <array>
#include <span>

#include "fall_line/sim/state.hpp"
#include "racing/tape.hpp"

namespace fall_line::sim {

inline constexpr std::array<ge::racing::TapeAxis, 4> kTapeAxes{{
    {"steer", ge::racing::AxisKind::Signed},
    {"tuck", ge::racing::AxisKind::Lever},
    {"brake", ge::racing::AxisKind::Lever},
    {"flags", ge::racing::AxisKind::Flags},
}};
inline constexpr ge::racing::TapeSchema kTapeSchema{kTapeAxes};

inline void controls_to_axes(const Controls& c, std::span<double, 4> out) {
  out[0] = ge::racing::snap_axis(c.ski.steer, ge::racing::AxisKind::Signed);
  out[1] = ge::racing::snap_axis(c.ski.tuck, ge::racing::AxisKind::Lever);
  out[2] = ge::racing::snap_axis(c.ski.brake, ge::racing::AxisKind::Lever);
  out[3] = (c.ski.jump ? 1 : 0) + (c.reset ? 2 : 0);
}

inline Controls controls_from_axes(std::span<const double, 4> a) {
  Controls c;
  c.ski.steer = a[0];
  c.ski.tuck = a[1];
  c.ski.brake = a[2];
  const int flags = static_cast<int>(a[3]);
  c.ski.jump = (flags & 1) != 0;
  c.reset = (flags & 2) != 0;
  return c;
}

// `c` with every axis on the tape's grid.
inline Controls snap_controls(const Controls& c) {
  std::array<double, 4> a{};
  controls_to_axes(c, a);
  return controls_from_axes(a);
}

}  // namespace fall_line::sim
