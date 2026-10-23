// THE KNOBS BY NAME: every field of `MoveSpec` as a row a console, a cfg file, a lab's flag or a
// tuning card reads and writes by name, with its unit, a range and a line of help. The rows are
// the one place a knob's name is written; `scale_lengths` reads their units, the console binds
// their fields (`number_field`, `flag_field`, `count_field`), and the `move-*` labs take any of
// them as `--set=name=value`.
//
// NAMES are bare (`gravity`, `maxspeed`, `airaccelerate`): a game puts its own owner prefix in
// front when it binds them (`sv_` for a server's rules, so `sv_gravity`; docs/movement.md §
// Knobs as cvars). They are lower case, one word, no separators, as console names are typed.
//
// ge:summary Character movement's knobs by name: unit, range, help; get, set, parse, cvar fields
// ge:keywords move movement knobs knob cvar cvars console tuning names table set_knob knob_value
// ge:keywords number_field flag_field count_field find_knob apply_knobs knob_text sv_gravity
// ge:skill move
// ge:test tests/move_step_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "move/spec.hpp"

namespace ge::move {

// How a knob's value scales with the length unit: not at all, or as a length (L, L/s, L/s^2 all
// scale by the same factor when only the length unit changes).
enum class Dim : uint8_t { none, length };

enum class KnobKind : uint8_t { number, flag, count };

struct Knob {
  const char* name;
  const char* unit;  // "L", "L/s", "L/s^2", "1/s", "s", "share", "" (a flag or a count)
  const char* help;
  double min, max;
  KnobKind kind;
  Dim dim;
  double MoveSpec::*number;  // kind number
  bool MoveSpec::*flag;      // kind flag
  int32_t MoveSpec::*count;  // kind count
};

// Every knob, in MoveSpec's order.
std::span<const Knob> knobs();
// By name (exact, lower case); null when none.
const Knob* find_knob(std::string_view name);

// The value as a number (a flag 0 or 1).
double knob_value(const MoveSpec& spec, const Knob& knob);
// Set, clamped to the knob's range (a flag: nonzero is on; a count rounded half away from zero).
void set_knob(MoveSpec& spec, const Knob& knob, double value);

// The field a console binds, or null when the knob is of another kind.
double* number_field(MoveSpec& spec, const Knob& knob);
bool* flag_field(MoveSpec& spec, const Knob& knob);
int32_t* count_field(MoveSpec& spec, const Knob& knob);

// `name=value` pairs separated by commas or spaces ("gravity=600,airaccelerate=10") set on
// `spec`. False, with the reason and the next action in `error`, on an unknown name or a value
// that is not a number; the knobs before it stay set.
bool apply_knobs(MoveSpec& spec, std::string_view text, std::string& error);

// Every knob that differs from `base`, as `name=value` pairs joined by commas ("" when none):
// what a lab prints so a before and an after say which row they ran.
std::string knob_text(const MoveSpec& spec, const MoveSpec& base);

}  // namespace ge::move
