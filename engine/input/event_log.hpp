// THE EVENT LOG: a session's device input written down frame by frame, so what a player did on
// a real pad, wheel or keyboard is replayed here, on any machine and with no device, to the
// same input state and the same action values. It sits one level under the play contract's
// input log (play/inputs.hpp), which records the AXES a run was stepped on: this one records
// the DEVICES, so a binding, a dead zone or a rebinding screen can be changed and the session
// read again through it. It is text, so an agent writes one by hand as a script.
//
//   ge-events 1
//   device 1 gamepad playstation fff1:0003 features=rumble,gyro axes=6 buttons=21 "a pad"
//   frame 0
//   0 1 added
//   frame 0.016667
//   0.01 1 pad_button south down
//   0.012 1 pad_axis left_x -0.5
//   0.013 2 key a down
//   0.014 3 motion 1.5 -2 0 0 raw
//   0.015 4 touch 0 down 0.25 0.8 1
//
// A `frame <t>` line opens a frame (its time, s, on the backend's clock: the frame's dt is the
// gap to the next); every event line after it, until the next, is in that frame, in order:
// `<t> <device> <type> <code> <values...>`, the type and code spelled as names.hpp spells them.
// A `device` line describes a device before the frame it is plugged in. Times are seconds
// written exactly; `#` starts a comment.
//
// ge:summary The event log: a session's device events as text, frame by frame; record, replay
// ge:keywords input event-log record replay recording playback script text frames reproduce
// ge:keywords ge-events write_events read_events frame device-line
// ge:skill input
// ge:test tests/input_sim_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "input/state.hpp"

namespace ge::input {

struct LoggedFrame {
  uint64_t t_ns = 0;
  Queue q;
};

struct EventLog {
  std::vector<LoggedFrame> frames;
  void add(uint64_t t_ns, const Queue& q) { frames.push_back({t_ns, q}); }
};

std::string write_events(const EventLog& log);
bool read_events(std::string_view text, EventLog& log, std::string& error);

std::string feature_names(uint32_t features);  // "rumble,gyro"
bool features_from_names(std::string_view names, uint32_t& features);

}  // namespace ge::input
