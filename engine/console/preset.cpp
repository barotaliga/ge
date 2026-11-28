// Presets: the table read as a grid, the sets, the computed level, the command and the check.
//
// ge:summary Presets' definitions: the grid's words, the sets, the level computed, the command
// ge:keywords preset apply_preset preset_level nearest_level add_preset_command check_preset
// ge:skill console
// ge:test tests/console_preset_test.cpp
#include "console/preset.hpp"

#include <cmath>

namespace ge::console {

std::string_view preset_value(const PresetRow& row, int level) {
  const std::string_view v = row.values;
  size_t i = 0;
  for (int at = 0;; ++at) {
    while (i < v.size() && v[i] == ' ') ++i;
    if (i == v.size()) return {};
    size_t end = i;
    while (end < v.size() && v[end] != ' ') ++end;
    if (at == level) return v.substr(i, end - i);
    i = end;
  }
}

int apply_preset(Cvars& cvars, const PresetTable& table, int level, Source source,
                 std::vector<std::string>* refused) {
  if (level < 0 || level >= table.levels) return -1;
  int changed = 0;
  for (const PresetRow& row : table.rows) {
    const SetResult r = cvars.set(row.cvar, preset_value(row, level), source);
    if (r == SetResult::changed) {
      ++changed;
    } else if (r != SetResult::same && refused != nullptr) {
      const Cvar* c = cvars.find(row.cvar);
      refused->push_back(c != nullptr && c->lock != nullptr ? c->lock->why
                                                            : std::string(row.cvar) + " was kept.");
    }
  }
  return changed;
}

bool preset_matches(const Cvars& cvars, std::string_view name, std::string_view value) {
  const Cvar* c = cvars.find(name);
  if (c == nullptr) return false;
  double want = 0;
  if (c->kind != CvarKind::text && parse_number(value, want)) return c->number == want;
  return c->value == value;
}

namespace {

int misses(const Cvars& cvars, const PresetTable& table, int level,
           std::vector<std::string_view>* away) {
  int n = 0;
  for (const PresetRow& row : table.rows) {
    if (preset_matches(cvars, row.cvar, preset_value(row, level))) continue;
    ++n;
    if (away != nullptr) away->push_back(row.cvar);
  }
  return n;
}

}  // namespace

int preset_level(const Cvars& cvars, const PresetTable& table) {
  for (int level = 0; level < table.levels; ++level)
    if (misses(cvars, table, level, nullptr) == 0) return level;
  return -1;
}

int nearest_level(const Cvars& cvars, const PresetTable& table,
                  std::vector<std::string_view>* away) {
  int best = 0, fewest = -1;
  for (int level = 0; level < table.levels; ++level) {
    const int n = misses(cvars, table, level, nullptr);
    if (fewest < 0 || n < fewest) {
      best = level;
      fewest = n;
    }
  }
  if (away != nullptr) misses(cvars, table, best, away);
  return best;
}

void add_preset_command(Console& con, const PresetTable& table, std::string_view help) {
  con.add_command(table.name, help, [&table](Console& k, const Args& a) {
    const std::string name(table.name);
    const std::string top = std::to_string(table.levels - 1);
    if (a.words.size() < 2) {
      const int level = preset_level(k.cvars(), table);
      if (level >= 0) {
        k.print(name + " is " + std::to_string(level) + ".");
        return;
      }
      std::vector<std::string_view> away;
      const int near = nearest_level(k.cvars(), table, &away);
      std::string rows;
      for (size_t i = 0; i < away.size(); ++i)
        rows += (i == 0 ? "" : i + 1 == away.size() ? " and " : ", ") + std::string(away[i]);
      k.print(name + " is custom: nearest is " + std::to_string(near) + ", but for " + rows + ".");
      return;
    }
    double x = 0;
    if (a.words.size() > 2 || !parse_number(a.words[1], x) || x != std::floor(x) || x < 0 ||
        x >= table.levels) {
      k.print(name + " takes a level from 0 to " + top + ".");
      return;
    }
    std::vector<std::string> refused;
    const int changed = apply_preset(k.cvars(), table, static_cast<int>(x), a.source, &refused);
    for (const std::string& line : refused) k.print(line);
    k.print(name + " " + a.words[1] + ": " + std::to_string(changed) +
            (changed == 1 ? " setting" : " settings") + " changed.");
  });
}

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
