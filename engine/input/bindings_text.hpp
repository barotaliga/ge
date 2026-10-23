// THE BINDINGS FILE: an action map's contexts and bindings as text, so a player's rebinding is
// saved and loaded, a game ships its defaults as a file an agent can read, and a lab prints what
// is bound. The actions themselves are the game's (`ActionMap::defs`, in code); the file binds
// sources to them by name.
//
//   ge-bindings 1
//   context ride
//   bind steer key:a value=-1 ramp=6,9
//   bind steer key:d ramp=6,9
//   bind steer pad:left_x deadzone=0.1,0.97 curve=1.5
//   bind steer role:steer
//   bind tuck pad:right_trigger
//   bind move pad:left_stick zone=scaled_radial deadzone=0.12,0.97
//   bind look mouse:x value=0.022
//   bind look mouse:y component=y value=-0.022
//   bind sprint key:w with=key:left_shift
//   bind shift_up joy:button4 device=046d:c24f
//   context console blocking off
//
// SOURCES: `key:<name>` (names.hpp), `mouse:left|middle|right|back|forward|x|y|wheel_x|wheel_y`,
// `pad:<button>`, `pad:<axis>`, `pad:left_stick|right_stick`, `joy:button<N>|axis<N>|hat<N>_up`
// (`_right`, `_down`, `_left`), `gyro:x|y|z`, `touch:<control>`, `role:<role>` (device_db.hpp).
// An axis source takes a trailing `+` or `-` to read one half of it (`pad:left_x-`).
//
// OPTIONS (each written only when it is not its default): `value=`, `component=x|y`,
// `deadzone=<inner>,<outer>`, `curve=<exponent>`, `anti=`, `zone=<zone>` (a stick's),
// `threshold=<on>,<off>`, `ramp=<attack>,<release>`, `rate=`, `with=<source>`, `device=<key>`.
// A context's words: `blocking`, `off`.
//
// ge:summary The bindings file: an action map's contexts and bindings as text, sources as words
// ge:keywords input bindings binding-file text format save load rebind config keymap source-text
// ge:keywords parse_source source_text write_bindings read_bindings ge-bindings
// ge:skill input
// ge:test tests/input_actions_test.cpp
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "input/actions.hpp"

namespace ge::input {

std::string source_text(const Source& s);
std::optional<Source> parse_source(std::string_view word);

std::string write_bindings(const ActionMap& map);
// Replace `map`'s contexts with the file's (its defs stay). On a mistake `map` is unchanged and
// `error` names the line and what to write instead.
bool read_bindings(std::string_view text, ActionMap& map, std::string& error);

}  // namespace ge::input
