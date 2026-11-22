// A server's settings sheet (sheet.hpp states the rows, the wire and the choices).
//
// ge:summary The settings sheet's rows as info strings, the parts taken, the choices, the lines
// ge:keywords sheet_row_text sheet_row_read sheet_take sheet_choices sheet_words_of
// ge:skill net
// ge:test tests/server_sheet_test.cpp
#include "server/sheet.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>

#include "console/words.hpp"
#include "core/num_text.hpp"

namespace ge::server {

namespace {

struct Field {
  char key;
  std::string SheetRow::*member;
};
// The row's fields as the wire's keys, help and values last (the ones dropped to fit).
constexpr std::array<Field, 7> kFields = {{{'n', &SheetRow::name},
                                           {'w', &SheetRow::words},
                                           {'g', &SheetRow::group},
                                           {'v', &SheetRow::value},
                                           {'l', &SheetRow::line},
                                           {'o', &SheetRow::values},
                                           {'h', &SheetRow::help}}};

void add_pair(std::string& out, char key, std::string_view value) {
  out += '\\';
  out += key;
  out += '\\';
  for (const char c : value) out += c == '\\' ? '/' : c;
}

// A number range word `a..b` (either end N: no bound that way).
bool range_word(std::string_view w, double& lo, double& hi) {
  const size_t dots = w.find("..");
  if (dots == std::string_view::npos) return false;
  const std::string_view a = w.substr(0, dots), b = w.substr(dots + 2);
  lo = -std::numeric_limits<double>::infinity();
  hi = std::numeric_limits<double>::infinity();
  return (a == "N" || ge::console::parse_number(a, lo)) &&
         (b == "N" || ge::console::parse_number(b, hi)) && lo <= hi;
}

std::string number_text(double v) {
  if (v == std::floor(v) && std::fabs(v) < 1e9) return std::to_string(static_cast<long long>(v));
  return ge::core::num::to_string(v);
}

// The meaning without the default's mark, for a menu's line.
std::string plain_meaning(std::string_view m) {
  std::string s(m);
  const std::string mark = " (the default)";
  if (const size_t at = s.find(mark); at != std::string::npos) s.erase(at, mark.size());
  return s;
}

constexpr double kLadder[] = {1,  2,  3,  4,   5,   10,  15,  20,  25,  30,  40,
                              50, 60, 75, 100, 150, 200, 250, 300, 500, 1000};

}  // namespace

const char* sheet_how_name(SheetHow how) {
  switch (how) {
    case SheetHow::direct:
      return "direct";
    case SheetHow::vote:
      return "vote";
    case SheetHow::admin:
      return "admin";
    case SheetHow::fixed:
      break;
  }
  return "fixed";
}

SheetHow sheet_how_named(std::string_view word) {
  if (word == "direct") return SheetHow::direct;
  if (word == "vote") return SheetHow::vote;
  if (word == "admin") return SheetHow::admin;
  return SheetHow::fixed;
}

const char* sheet_mark(SheetHow how) { return how == SheetHow::vote ? "*" : ""; }

std::string sheet_row_text(const SheetRow& row, size_t most) {
  for (size_t keep = kFields.size(); keep >= kFields.size() - 2; keep--) {
    std::string out;
    for (size_t i = 0; i < keep; i++) add_pair(out, kFields[i].key, row.*kFields[i].member);
    add_pair(out, 'm', sheet_how_name(row.how));
    if (most == 0 || out.size() <= most || keep == kFields.size() - 2) return out;
  }
  return {};
}

bool sheet_row_read(std::string_view text, SheetRow& out) {
  out = SheetRow{};
  size_t at = 0;
  while (at < text.size() && text[at] == '\\') {
    const size_t key_end = text.find('\\', at + 1);
    if (key_end == std::string_view::npos) break;
    const size_t value_end = std::min(text.find('\\', key_end + 1), text.size());
    const std::string_view key = text.substr(at + 1, key_end - at - 1);
    const std::string_view value = text.substr(key_end + 1, value_end - key_end - 1);
    at = value_end;
    if (key.size() != 1) continue;  // a later field this reader does not know
    if (key[0] == 'm') {
      out.how = sheet_how_named(value);
      continue;
    }
    for (const Field& f : kFields)
      if (f.key == key[0]) out.*f.member = std::string(value);
  }
  return !out.name.empty();
}

void sheet_put(SettingsSheet& sheet, SheetRow row) {
  for (SheetRow& r : sheet.rows)
    if (r.name == row.name) {
      r = std::move(row);
      sheet.revision++;
      return;
    }
  sheet.rows.push_back(std::move(row));
  sheet.revision++;
}

void sheet_take(SettingsSheet& sheet, int part, std::string_view text) {
  switch (static_cast<SheetPart>(part)) {
    case SheetPart::begin:
      sheet.rows.clear();
      sheet.whole = false;
      sheet.revision++;
      break;
    case SheetPart::row:
      if (SheetRow row; sheet_row_read(text, row)) sheet_put(sheet, std::move(row));
      break;
    case SheetPart::end:
      sheet.whole = true;
      sheet.asked = false;
      sheet.revision++;
      break;
  }
}

const SheetRow* sheet_find(const SettingsSheet& sheet, std::string_view name) {
  for (const SheetRow& r : sheet.rows)
    if (r.name == name) return &r;
  return nullptr;
}

std::vector<std::string> sheet_groups(const SettingsSheet& sheet) {
  std::vector<std::string> out;
  for (const SheetRow& r : sheet.rows)
    if (!r.group.empty() && std::find(out.begin(), out.end(), r.group) == out.end())
      out.push_back(r.group);
  return out;
}

std::string sheet_change_line(const SheetRow& row, std::string_view value) {
  std::string v(value);
  if (v.empty() || v.find(' ') != std::string::npos) v = "\"" + v + "\"";
  std::string line = row.line;
  if (line.empty())
    line = row.how == SheetHow::vote ? "vote " + row.name + " %v" : row.name + " %v";
  if (const size_t at = line.find("%v"); at != std::string::npos) line.replace(at, 2, v);
  return line;  // a line with no `%v` is a word that turns the setting on its own
}

std::vector<std::string> sheet_change_lines(const SheetRow& row, std::string_view value) {
  const size_t bar = row.line.find('|');
  if (bar == std::string::npos) {
    if (value == row.value) return {};
    return {sheet_change_line(row, value)};
  }
  const std::string up = row.line.substr(0, bar), down = row.line.substr(bar + 1);
  const std::vector<SheetChoice> choices = sheet_choices(row);
  const auto index = [&choices](std::string_view v) {
    for (size_t i = 0; i < choices.size(); i++)
      if (choices[i].value == v) return static_cast<long>(i);
    return -1L;
  };
  const long from = index(row.value), to = index(value);
  // A two-way switch whose state is not known yet: one press turns it.
  if (from < 0 && to >= 0 && choices.size() == 2) return {up};
  if (from < 0 || to < 0 || from == to) return {};
  const long n = static_cast<long>(choices.size());
  long steps = to - from;
  if (down.empty() && steps < 0) steps += n;
  std::vector<std::string> out;
  for (long i = 0; i < (steps < 0 ? -steps : steps); i++) out.push_back(steps > 0 ? up : down);
  return out;
}

std::vector<SheetChoice> sheet_choices(const SheetRow& row) {
  std::vector<SheetChoice> out;
  const auto has = [&out](std::string_view v) {
    return std::any_of(out.begin(), out.end(), [v](const SheetChoice& c) { return c.value == v; });
  };
  std::vector<ge::console::ValueEntry> entries;
  if (ge::console::parse_values(row.values, entries)) {
    for (const ge::console::ValueEntry& e : entries) {
      if (e.pattern == "N" || e.pattern == "TEXT" || e.pattern.find(' ') != std::string::npos)
        continue;
      if (e.pattern == "\"\"") {
        if (!has("")) out.push_back({"", plain_meaning(e.meaning)});
        continue;
      }
      double lo = 0, hi = 0;
      if (!range_word(e.pattern, lo, hi)) {
        if (!has(e.pattern)) out.push_back({e.pattern, plain_meaning(e.meaning)});
        continue;
      }
      // A range: every whole number when it is short, else the ladder inside it.
      if (std::isfinite(lo) && std::isfinite(hi) && hi - lo <= 32) {
        for (double v = std::ceil(lo); v <= hi; v += 1)
          if (const std::string t = number_text(v); !has(t)) out.push_back({t, {}});
        continue;
      }
      if (std::isfinite(lo))
        if (const std::string t = number_text(lo); !has(t)) out.push_back({t, {}});
      for (const double v : kLadder)
        if (v > lo && v <= hi)
          if (const std::string t = number_text(v); !has(t)) out.push_back({t, {}});
    }
  }
  if (!row.value.empty() && !has(row.value)) {
    // The value now among the numbers where it falls, else at the end.
    double now = 0;
    size_t at = out.size();
    if (ge::console::parse_number(row.value, now))
      for (size_t i = 0; i < out.size(); i++)
        if (double v = 0;
            out[i].meaning.empty() && ge::console::parse_number(out[i].value, v) && v > now) {
          at = i;
          break;
        }
    out.insert(out.begin() + static_cast<std::ptrdiff_t>(at),
               SheetChoice{row.value, sheet_meaning(row, row.value)});
  }
  return out;
}

std::string sheet_meaning(const SheetRow& row, std::string_view value) {
  std::vector<ge::console::ValueEntry> entries;
  if (!ge::console::parse_values(row.values, entries)) return {};
  for (const ge::console::ValueEntry& e : entries)
    if (ge::console::pattern_fits(e.pattern, value)) return plain_meaning(e.meaning);
  return {};
}

SheetHow sheet_how_by_vote(const VoteSettings& settings, std::string_view name) {
  return settings.enabled && vote_permitted(settings, name) == VoteWhy::ok ? SheetHow::vote
                                                                           : SheetHow::fixed;
}

std::string sheet_words_of(std::string_view name) {
  struct Known {
    std::string_view name, words;
  };
  static constexpr Known kKnown[] = {
      {"map", "Map"},
      {"mode", "Mode"},
      {"timelimit", "Time limit"},
      {"fraglimit", "Frag limit"},
      {"teamplay", "Teamplay"},
      {"deathmatch", "Deathmatch mode"},
      {"maxclients", "Players"},
      {"maxspectators", "Spectators"},
      {"dmflags", "Deathmatch flags"},
      {"capturelimit", "Capture limit"},
      {"samelevel", "Same map again"},
  };
  for (const Known& k : kKnown)
    if (k.name == name) return std::string(k.words);
  std::string_view s = name;
  for (const std::string_view prefix : {"sv_", "pm_", "k_", "g_", "cl_"})
    if (s.starts_with(prefix) && s.size() > prefix.size()) {
      s.remove_prefix(prefix.size());
      break;
    }
  std::string out(s);
  for (char& c : out)
    if (c == '_') c = ' ';
  if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  return out;
}

}  // namespace ge::server
