// REBINDING BY PRESSING: a rebinding screen asks "press the key for JUMP" and takes the next
// thing the player does as the source. `begin_capture` notes how every axis rests (a pedal rests
// at one end, a worn stick off centre), then `poll_capture`, once a frame, answers with the
// first deliberate input: a key, a mouse button, a pad or joystick button, a hat, an axis moved
// more than half its range from its rest (as the half it moved toward, for a button or an axis
// action), the mouse moved far enough or a touch control, when the action takes it. ESCAPE
// cancels. A source from a joystick-like device carries that device's model key, so a wheel's
// button 4 binds that wheel and not every joystick.
//
// `conflicts` names the actions a source is already bound to in a context, so the screen can
// say "already on STEER" and offer to swap; `rebind` puts the source on an action, replacing
// its binding at `slot` or adding one.
//
// ge:summary Rebinding by pressing: capture the next deliberate input, conflicts, rebind
// ge:keywords input rebind remap capture listen press-a-key prompt conflict swap binding-slot
// ge:keywords begin_capture poll_capture conflicts rebind cancel escape
// ge:skill input
// ge:test tests/input_actions_test.cpp
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "input/actions.hpp"

namespace ge::input {

struct Capture {
  bool active = false;
  ActionKind for_kind = ActionKind::button;
  struct Rest {
    DeviceId id = 0;
    std::array<float, kMaxAxes> axes{};
  };
  std::array<Rest, kMaxDevices> rest{};
  double mouse_x = 0, mouse_y = 0;  // the mouse's travel since the capture began
  float mouse_travel = 40;          // what counts as a deliberate move, counts
};

enum class CaptureResult : uint8_t { waiting, captured, cancelled };

void begin_capture(Capture& c, const InputState& s, ActionKind for_kind);
// Watch this frame; on `captured`, `out` is the source (and `device` its model key, "" for any).
CaptureResult poll_capture(Capture& c, const InputState& s, Source& out, std::string& device);

// The actions `source` is bound to in `context` (their names).
std::vector<std::string> conflicts(const ActionMap& map, std::string_view context,
                                   const Source& source);
// Put `source` on `action` in `context`: replace the action's `slot`-th binding there, or add a
// binding when it has fewer. False when the action or the context does not exist.
bool rebind(ActionMap& map, std::string_view context, std::string_view action, size_t slot,
            const Source& source, const std::string& device);

}  // namespace ge::input
