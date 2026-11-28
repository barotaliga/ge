// PRESETS: one setting that stands for a group of others, as a COMMAND that writes them. A game
// states a table (`PresetTable`): a row a cvar, a value for each level, and `apply_preset` sets
// every row's cvar to its level's value, as the player typing them would. Nothing is kept beside
// the cvars: no override, no layer, no value of the preset itself. A config saves the cvars it
// wrote, and a player who then changes one has simply changed that cvar.
//
// - THE LEVEL IS COMPUTED (`preset_level`): the level whose every row matches the cvars as they
//   stand (numbers compared as numbers, text exactly), else none: the player's own mix, which a
//   menu shows as CUSTOM. `nearest_level` names the level fewest rows away, for the console's
//   answer.
// - THE COMMAND (`add_preset_command`): `<name> <level>` applies it and says what it changed;
//   `<name>` alone answers the level the cvars make, or that they make none and which rows keep
//   the nearest level away.
// - THE TABLE IS DATA, read as a grid: a row's values are one string of words, level 0 first
//   (`{"ce_ao", "0 0 0 0 0 0 1 1 1 1 1"}`), so a level is a column and a rename of a cvar is one
//   word in one place. `check_preset` holds a table to a console: every row's cvar registered,
//   every row a word for each level, every value one the cvar keeps as written.
//
// ge:summary Presets: one command that writes a group of cvars by level, the level computed back
// ge:keywords preset presets quality-preset level apply_preset preset_level nearest_level
// ge:keywords add_preset_command check_preset preset-table custom computed-level graphics-preset
// ge:keywords quality-slider command-writes-cvars no-override
// ge:skill console
// ge:test tests/console_preset_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "console/cvar.hpp"

namespace ge::console {

// One cvar a preset owns, and its value at each level: words apart, level 0 first.
struct PresetRow {
  std::string_view cvar;
  std::string_view values;
};

struct PresetTable {
  std::string_view name;  // the command's ("r_quality")
  int levels = 0;         // 0 .. levels - 1
  std::span<const PresetRow> rows;
};

// The value `row` gives at `level` ("" when it has no word there).
std::string_view preset_value(const PresetRow& row, int level);

// Every row's cvar set to its value at `level` from `source`; the rows a set refused (an unknown
// cvar, a lock) named in `refused`, one line each. The count of cvars it changed; -1 for a level
// out of range (nothing set).
int apply_preset(Cvars& cvars, const PresetTable& table, int level, Source source,
                 std::vector<std::string>* refused = nullptr);

// Whether the cvar `name` holds `value` as a preset compares them (numbers as numbers).
bool preset_matches(const Cvars& cvars, std::string_view name, std::string_view value);
// The level whose every row matches the cvars now, or -1 when none does (the player's own mix).
int preset_level(const Cvars& cvars, const PresetTable& table);
// The level with the fewest rows that do not match, the lowest on a tie; `away` (when given) gets
// those rows' cvars.
int nearest_level(const Cvars& cvars, const PresetTable& table,
                  std::vector<std::string_view>* away = nullptr);

// The command `table.name` on `con` (EVERY PRESET above): with a level it applies it from the
// command's source, without one it answers the level the cvars make. `help` is its help phrase.
void add_preset_command(Console& con, const PresetTable& table, std::string_view help);

// Faults in `table` against `cvars`, one line each naming the row and the next action: a cvar
// nothing registered, a row without a word for each level, a value the cvar would not keep as
// written (clamped, or not a number for a number). Empty when it holds.
std::vector<std::string> check_preset(const Cvars& cvars, const PresetTable& table);

}  // namespace ge::console
