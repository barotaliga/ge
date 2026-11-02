// The team's words (talk.hpp): a message written from its parts and read back into them.
//
// ge:summary Team messages written and read: the words, the status, the place, the timers
// ge:keywords say_text parse_say split_team_line normalise colour-codes status tokens
// ge:skill shooter bots
// ge:test tests/shooters_edge_talk_test.cpp
#include "shooters_edge/bots/talk.hpp"

#include <array>
#include <cctype>
#include <cstdlib>
#include <vector>

namespace shooters_edge::bots {

namespace {

constexpr std::array<std::string_view, 21> kItems = {
    "ra", "ya",  "ga",      "mh",    "quad",  "pent",   "ring", "rl",   "gl",   "lg",    "sng",
    "ng", "ssg", "rockets", "cells", "nails", "shells", "rox",  "suit", "pack", "health"};
constexpr std::array<std::string_view, 8> kWeapons = {"rl", "lg",  "gl", "sng",
                                                      "ng", "ssg", "sg", "axe"};

bool is_digits(std::string_view w) {
  if (w.empty()) return false;
  for (char c : w)
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  return true;
}

int to_int(std::string_view w) { return std::atoi(std::string(w).c_str()); }

bool weapon_word(std::string_view w) {
  for (std::string_view k : kWeapons)
    if (w == k) return true;
  return false;
}

// The word as an item word ("mega" is "mh", "rockets" are "rox"), or "" when not one.
std::string_view as_item(std::string_view w) {
  if (w == "mega") return "mh";
  if (w == "rockets") return "rox";
  if (w == "quaded" || w == "quadded") return "";
  for (std::string_view k : kItems)
    if (w == k) return k;
  return "";
}

// The text with its colour codes and braces gone, its high-bit letters plain and lower case, the
// bracket macros as plain brackets.
std::string normalise(std::string_view in) {
  std::string out;
  for (size_t i = 0; i < in.size(); i++) {
    const auto c = static_cast<unsigned char>(in[i]);
    if (c == '&' && i + 1 < in.size() && (in[i + 1] == 'c' || in[i + 1] == 'C')) {
      i += 4;  // "&cRGB"
      continue;
    }
    if (c == '&' && i + 1 < in.size() && (in[i + 1] == 'r' || in[i + 1] == 'R')) {
      i++;
      continue;
    }
    if (c == '$' && i + 1 < in.size() && (in[i + 1] == '[' || in[i + 1] == ']')) continue;
    if (c == '{' || c == '}') continue;
    const auto plain = static_cast<unsigned char>(c & 0x7f);
    if (plain < 32) {
      out += ' ';
      continue;
    }
    out += static_cast<char>(std::tolower(plain));
  }
  return out;
}

std::vector<std::string_view> words_of(std::string_view s) {
  std::vector<std::string_view> w;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == ',' || s[i] == '!' || s[i] == '.' ||
                            s[i] == '\t' || s[i] == '-' || s[i] == '"'))
      i++;
    const size_t from = i;
    while (i < s.size() && s[i] != ' ' && s[i] != ',' && s[i] != '!' && s[i] != '.' &&
           s[i] != '\t' && s[i] != '"')
      i++;
    if (i > from) w.push_back(s.substr(from, i - from));
  }
  return w;
}

// A status word ("ra:150", "h:100", "150/100", a weapon, "quaded"); true when it was one.
bool read_status(std::string_view w, SayStatus& st) {
  const size_t colon = w.find(':');
  if (colon != std::string_view::npos && colon + 1 < w.size() && is_digits(w.substr(colon + 1))) {
    const std::string_view k = w.substr(0, colon);
    const int v = to_int(w.substr(colon + 1));
    if (k == "h") {
      st.health = v;
    } else {
      st.armor = v;
      if (k == "ra" || k == "ya" || k == "ga") st.armor_kind = std::string(k);
    }
    return true;
  }
  const size_t slash = w.find('/');
  if (slash != std::string_view::npos && is_digits(w.substr(0, slash)) &&
      is_digits(w.substr(slash + 1))) {
    st.armor = to_int(w.substr(0, slash));
    st.health = to_int(w.substr(slash + 1));
    return true;
  }
  if (w == "quaded" || w == "quadded") {
    st.quad = true;
    return true;
  }
  return false;
}

std::string status_text(const SayStatus& st) {
  std::string s = (st.armor_kind.empty() ? std::string("a") : st.armor_kind) + ":" +
                  std::to_string(st.armor < 0 ? 0 : st.armor) +
                  " h:" + std::to_string(st.health < 0 ? 0 : st.health);
  if (!st.weapon.empty()) s += " " + st.weapon;
  if (st.quad) s += " quaded";
  return s;
}

}  // namespace

const char* say_kind_name(SayKind k) {
  switch (k) {
    case SayKind::none:
      return "none";
    case SayKind::report:
      return "report";
    case SayKind::took:
      return "took";
    case SayKind::safe:
      return "safe";
    case SayKind::lost:
      return "lost";
    case SayKind::help:
      return "help";
    case SayKind::coming:
      return "coming";
    case SayKind::point:
      return "point";
    case SayKind::enemy:
      return "enemy";
    case SayKind::enemy_power:
      return "enemy power";
    case SayKind::have:
      return "have";
    case SayKind::get:
      return "get";
    case SayKind::over:
      return "over";
    case SayKind::timer:
      break;
  }
  return "timer";
}

bool item_word(std::string_view w) { return !as_item(w).empty(); }

std::string say_text(const Said& s) {
  const std::string at = s.loc.empty() ? std::string() : " [" + s.loc + "]";
  switch (s.kind) {
    case SayKind::none:
      return std::string();
    case SayKind::report:
      return status_text(s.status) + at;
    case SayKind::took:
      return "took " + s.item + at;
    case SayKind::safe:
      return "safe" + at + " " + status_text(s.status);
    case SayKind::lost:
      return "lost" + at;
    case SayKind::help:
      return "help" + at + " " + status_text(s.status);
    case SayKind::coming:
      return "coming" + (s.item.empty() ? std::string() : " " + s.item) + at;
    case SayKind::point:
      return s.item + " at" + at;
    case SayKind::enemy:
      return "enemy at" + at;
    case SayKind::enemy_power:
      return "enemy " + s.item + at;
    case SayKind::have:
      return "have " + s.item + at;
    case SayKind::get:
      return "get " + s.item;
    case SayKind::over:
      return s.item + " over";
    case SayKind::timer:
      break;
  }
  return s.item + " in " + std::to_string(s.secs);
}

Said parse_say(std::string_view text) {
  Said out;
  std::string t = normalise(text);
  // The place: the first bracketed run, taken out of the words.
  const size_t lb = t.find('[');
  if (lb != std::string::npos) {
    const size_t rb = t.find(']', lb);
    const size_t end = rb == std::string::npos ? t.size() : rb;
    std::string loc = t.substr(lb + 1, end - lb - 1);
    while (!loc.empty() && loc.front() == ' ') loc.erase(loc.begin());
    while (!loc.empty() && loc.back() == ' ') loc.pop_back();
    out.loc = loc;
    t.erase(lb, (rb == std::string::npos ? t.size() : rb + 1) - lb);
  }
  const std::vector<std::string_view> all = words_of(t);
  // The status words out, the rest kept in order.
  std::vector<std::string_view> w;
  for (std::string_view x : all)
    if (!read_status(x, out.status)) w.push_back(x);
  bool status_only = true;
  for (std::string_view x : w)
    if (weapon_word(x) && out.status.weapon.empty() && (out.status.health >= 0)) {
      out.status.weapon = std::string(x);
    } else {
      status_only = false;
    }
  const auto has = [&](std::initializer_list<std::string_view> ks) {
    for (std::string_view x : w)
      for (std::string_view k : ks)
        if (x == k) return true;
    return false;
  };
  const auto first_item = [&]() -> std::string {
    for (std::string_view x : w)
      if (item_word(x)) return std::string(as_item(x));
    return std::string();
  };
  // The word after `k` (or "" none).
  const auto after = [&](std::string_view k) -> std::string_view {
    for (size_t i = 0; i + 1 < w.size(); i++)
      if (w[i] == k) return w[i + 1];
    return {};
  };

  // A timer: an item and "in N" or "delayed N".
  for (size_t i = 0; i + 1 < w.size(); i++)
    if ((w[i] == "in" || w[i] == "delayed") && is_digits(w[i + 1]) && !first_item().empty()) {
      out.kind = SayKind::timer;
      out.item = first_item();
      out.secs = to_int(w[i + 1]);
      return out;
    }
  out.item = first_item();
  if (has({"took", "got"})) {
    out.kind = SayKind::took;
  } else if (has({"safe"})) {
    out.kind = SayKind::safe;
  } else if (has({"lost"})) {
    out.kind = SayKind::lost;
  } else if (has({"help"})) {
    out.kind = SayKind::help;
  } else if (has({"coming", "going", "omw"})) {
    out.kind = SayKind::coming;
  } else if (has({"enemy", "enemies", "en"})) {
    const std::string_view next = after(has({"enemy"}) ? "enemy" : "en");
    if (next == "quad" || next == "pent" || next == "ring") {
      out.kind = SayKind::enemy_power;
      out.item = std::string(next);
    } else {
      out.kind = SayKind::enemy;
      out.item.clear();
      if (out.loc.empty()) {
        const std::string_view where = after("at");
        if (!where.empty()) out.loc = std::string(as_item(where).empty() ? where : as_item(where));
      }
    }
  } else if (has({"have", "quaded"}) || out.status.quad) {
    out.kind = SayKind::have;
    if (out.item.empty()) out.item = "quad";
  } else if (has({"get"}) && !out.item.empty()) {
    out.kind = SayKind::get;
  } else if (has({"over", "dead", "done", "gone"}) && !out.item.empty()) {
    out.kind = SayKind::over;
  } else if (has({"at"}) && !out.item.empty()) {
    out.kind = SayKind::point;
    if (out.loc.empty()) {
      const std::string_view where = after("at");
      if (!where.empty()) out.loc = std::string(as_item(where).empty() ? where : as_item(where));
    }
  } else if (status_only && (out.status.health >= 0 || out.status.armor >= 0)) {
    out.kind = SayKind::report;
  }
  return out;
}

bool split_team_line(std::string_view line, std::string& name, std::string& text) {
  if (line.empty() || line[0] != '(') return false;
  const size_t close = line.find("): ");
  if (close == std::string_view::npos) return false;
  name = std::string(line.substr(1, close - 1));
  std::string_view rest = line.substr(close + 3);
  while (!rest.empty() && (rest.back() == '\n' || rest.back() == '\r')) rest.remove_suffix(1);
  text = std::string(rest);
  return true;
}

}  // namespace shooters_edge::bots
