// Binds, defined: the key-name table, modifier normalisation and the tables.
//
// ge:summary The bind tables' definitions and the key-name table
// ge:keywords bind key-name normalize_key known_key mode_name parse_mode
// ge:skill console
// ge:test tests/console_binds_test.cpp
#include "console/binds.hpp"

#include <algorithm>
#include <iterator>

#include "console/cvar.hpp"

namespace ge::console {

namespace {

constexpr std::string_view kNamed[] = {
    "space",    "enter",     "tab",       "escape",   "backspace", "up",       "down",
    "left",     "right",     "shift",     "ctrl",     "alt",       "ins",      "del",
    "home",     "end",       "pgup",      "pgdn",     "pause",     "capslock", "kp_enter",
    "kp_plus",  "kp_minus",  "kp_star",   "kp_slash", "kp_del",    "mwheelup", "mwheeldown",
    "pad_a",    "pad_b",     "pad_x",     "pad_y",    "pad_lb",    "pad_rb",   "pad_lt",
    "pad_rt",   "pad_back",  "pad_start", "pad_ls",   "pad_rs",    "pad_up",   "pad_down",
    "pad_left", "pad_right", "semicolon", "[",        "]",         ",",        ".",
    "/",        "\\",        "'",         "-",        "=",         "`",
};

bool numbered(std::string_view key, std::string_view prefix, int lo, int hi) {
  if (key.size() <= prefix.size() || key.substr(0, prefix.size()) != prefix) return false;
  const std::string_view digits = key.substr(prefix.size());
  if (digits.size() > 1 && digits[0] == '0') return false;
  int n = 0;
  for (char c : digits) {
    if (c < '0' || c > '9') return false;
    n = n * 10 + (c - '0');
    if (n > hi) return false;
  }
  return n >= lo;
}

constexpr std::string_view kModifiers[] = {"ctrl", "alt", "shift"};

// usage_key's table: HID usage 4 to 99 (the keyboard's main block, the F keys, the editing block,
// the arrows and the keypad); "" where no bind names the key.
constexpr std::string_view kUsageNames[] = {
    "a",        "b",        "c",         "d",
    "e",        "f",        "g",         "h",  // 4
    "i",        "j",        "k",         "l",
    "m",        "n",        "o",         "p",  // 12
    "q",        "r",        "s",         "t",
    "u",        "v",        "w",         "x",  // 20
    "y",        "z",        "1",         "2",
    "3",        "4",        "5",         "6",  // 28
    "7",        "8",        "9",         "0",
    "enter",    "escape",   "backspace", "tab",  // 36
    "space",    "-",        "=",         "[",
    "]",        "\\",       "",          "semicolon",  // 44
    "'",        "`",        ",",         ".",
    "/",        "capslock", "f1",        "f2",  // 52
    "f3",       "f4",       "f5",        "f6",
    "f7",       "f8",       "f9",        "f10",  // 60
    "f11",      "f12",      "",          "",
    "pause",    "ins",      "home",      "pgup",  // 68
    "del",      "end",      "pgdn",      "right",
    "left",     "down",     "up",        "",  // 76
    "kp_slash", "kp_star",  "kp_minus",  "kp_plus",
    "kp_enter", "kp_1",     "kp_2",      "kp_3",  // 84
    "kp_4",     "kp_5",     "kp_6",      "kp_7",
    "kp_8",     "kp_9",     "kp_0",      "kp_del",  // 92
};
constexpr uint16_t kFirstUsage = 4;

}  // namespace

std::string_view usage_key(uint16_t usage) {
  if (usage >= kFirstUsage && static_cast<size_t>(usage - kFirstUsage) < std::size(kUsageNames))
    return kUsageNames[usage - kFirstUsage];
  switch (usage) {
    case 224:
    case 228:
      return "ctrl";
    case 225:
    case 229:
      return "shift";
    case 226:
    case 230:
      return "alt";
    default:
      return {};
  }
}

std::string_view mode_name(BindMode m) {
  switch (m) {
    case BindMode::game:
      return "game";
    case BindMode::console:
      return "console";
    case BindMode::menu:
      return "menu";
    case BindMode::demo:
      return "demo";
  }
  return "game";
}

bool parse_mode(std::string_view name, BindMode& out) {
  for (int i = 0; i < kBindModes; ++i) {
    if (mode_name(static_cast<BindMode>(i)) == name) {
      out = static_cast<BindMode>(i);
      return true;
    }
  }
  return false;
}

bool known_key(std::string_view key) {
  if (key.size() == 1 && ((key[0] >= 'a' && key[0] <= 'z') || (key[0] >= '0' && key[0] <= '9')))
    return true;
  if (std::find(std::begin(kNamed), std::end(kNamed), key) != std::end(kNamed)) return true;
  return numbered(key, "f", 1, 12) || numbered(key, "mouse", 1, 8) || numbered(key, "kp_", 0, 9);
}

bool normalize_key(std::string_view name, std::string& out) {
  out.clear();
  const std::string s = lower(name);
  // The key is the last part; a `+` alone or at the end is the key "+" (not named: refused).
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    const size_t p = s.find('+', start);
    if (p == std::string::npos || p == s.size() - 1) {
      parts.push_back(s.substr(start));
      break;
    }
    parts.push_back(s.substr(start, p - start));
    start = p + 1;
  }
  const std::string& key = parts.back();
  if (!known_key(key)) return false;
  bool mods[3] = {false, false, false};
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    bool found = false;
    for (int m = 0; m < 3; ++m) {
      if (parts[i] == kModifiers[m]) {
        mods[m] = true;
        found = true;
      }
    }
    if (!found) return false;
  }
  for (int m = 0; m < 3; ++m)
    if (mods[m]) out += std::string(kModifiers[m]) + "+";
  out += key;
  return true;
}

const std::string& Binds::get(BindMode mode, std::string_view key) const {
  static const std::string kNone;
  const auto& t = tables_[static_cast<size_t>(mode)];
  const auto it = t.find(std::string(key));
  return it == t.end() ? kNone : it->second;
}

void Binds::set(BindMode mode, const std::string& key, std::string_view command) {
  tables_[static_cast<size_t>(mode)][key] = std::string(command);
}

bool Binds::remove(BindMode mode, const std::string& key) {
  return tables_[static_cast<size_t>(mode)].erase(key) > 0;
}

void Binds::clear() {
  for (auto& t : tables_) t.clear();
}

}  // namespace ge::console
