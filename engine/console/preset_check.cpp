// Presets' check: a table held to a console's cvars, each fault a line for the agent who writes
// the table (a test prints them), never for a player.
// voice: agent
//
// ge:summary Presets' check: every row's cvar registered, a word a level, values kept as written
// ge:keywords preset check_preset preset-table faults
// ge:skill console
// ge:test tests/console_preset_test.cpp
#include <cmath>

#include "console/preset.hpp"

namespace ge::console {

std::vector<std::string> check_preset(const Cvars& cvars, const PresetTable& table) {
  std::vector<std::string> faults;
  const std::string name(table.name);
  for (const PresetRow& row : table.rows) {
    const std::string at = name + "'s row " + std::string(row.cvar);
    const Cvar* c = cvars.find(row.cvar);
    if (c == nullptr) {
      faults.push_back(at +
                       ": no such cvar; next: register it, or rename the row to the cvar's "
                       "name now");
      continue;
    }
    if (!preset_value(row, table.levels).empty() || preset_value(row, table.levels - 1).empty()) {
      faults.push_back(at + ": needs exactly " + std::to_string(table.levels) +
                       " words, level 0 first; next: give each level one value");
      continue;
    }
    for (int level = 0; level < table.levels; ++level) {
      const std::string_view v = preset_value(row, level);
      if (c->kind == CvarKind::text) continue;
      double x = 0;
      const bool number = parse_number(v, x);
      const bool kept = number && x >= c->min && x <= c->max &&
                        (c->kind == CvarKind::number || x == std::floor(x)) &&
                        (c->kind != CvarKind::boolean || x == 0 || x == 1);
      if (!kept)
        faults.push_back(at + ": level " + std::to_string(level) + "'s \"" + std::string(v) +
                         "\" is not a value it keeps as written; next: write one inside its "
                         "range (help " +
                         std::string(row.cvar) + ")");
    }
  }
  return faults;
}

}  // namespace ge::console
