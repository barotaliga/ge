// THE DEVICES TO THE AXES: how a person at a keyboard, a pad or a wheel plays a game through
// the play contract. A game's actions (input/actions.hpp) are named as its spec's axes are
// ("steer", "tuck", "jump"); `bind_axes` pairs each axis with the action of its name, and once
// the frame's actions are evaluated, `axes_step` fills one value per axis for each fixed step
// the frame grants, by the axis's kind:
//
//   signed_unit  the action's value, -1 .. 1
//   lever        its value, 0 .. 1
//   toggle       1 while it is held
//   press        1 on ONE step only: the first step after it went down (a press is latched by
//                `axes_frame` and taken by the step that reads it), so a press is never lost
//                in a frame that grants no step and never repeated in one that grants three
//
// The values are snapped onto the axes' grids by the engine, as an agent's and a pilot's are,
// so the input log a person's run writes replays it exactly (session.hpp).
//
// ge:summary The devices to the axes: a game's actions paired with its spec's axes, per step
// ge:keywords play axes devices input actions bind_axes axes_step axes_frame latch press human
// ge:keywords keyboard gamepad wheel person player hands
// ge:skill agent-interface input
// ge:test tests/play_devices_test.cpp
#pragma once

#include <span>
#include <string>
#include <vector>

#include "input/actions.hpp"
#include "play/game.hpp"

namespace ge::play {

struct AxisFeed {
  int action = -1;  // the action of the axis's name (-1: none, the axis stays 0)
  AxisKind kind = AxisKind::signed_unit;
  bool latched = false;  // a press waiting for its step
};

struct DeviceAxes {
  std::vector<AxisFeed> feeds;  // one per axis, in the spec's order
};

// Pair each axis with the action of its name; `missing` lists the axes with none.
DeviceAxes bind_axes(const input::ActionMap& map, std::span<const Axis> axes,
                     std::vector<std::string>* missing = nullptr);
// Once a frame, after `evaluate`: latch the presses.
void axes_frame(DeviceAxes& d, const input::ActionValues& v);
// One step's values, one per axis (`out` sized to the axes).
void axes_step(DeviceAxes& d, const input::ActionValues& v, std::span<double> out);

}  // namespace ge::play
