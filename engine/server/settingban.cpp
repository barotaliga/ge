// Setting bans and user info bans (settingban.hpp).
//
// ge:summary cvarban_add's lists: the match forms, the query line, the words, the log line
// ge:keywords cvarban userinfoban setting_ban_matches setting_ban_command setting_ban_hits
// ge:skill net
// ge:test tests/server_settingban_test.cpp
#include "server/settingban.hpp"

#include <algorithm>
#include <cstdlib>

namespace ge::server {

namespace {

char low(char ch) { return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch; }

bool same(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (low(a[i]) != low(b[i])) return false;
  return true;
}

bool contains(std::string_view hay, std::string_view needle) {
  if (needle.empty()) return true;
  for (size_t i = 0; i + needle.size() <= hay.size(); i++)
    if (same(hay.substr(i, needle.size()), needle)) return true;
  return false;
}

double number(std::string_view s) {
  const std::string t(s);
  return std::strtod(t.c_str(), nullptr);  // an empty or wordy value reads as 0
}

bool space(char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; }

// The next word of `s` from `at` (a quoted word without its quotes); `at` moved past it.
std::string_view next_word(std::string_view s, size_t& at) {
  while (at < s.size() && space(s[at])) at++;
  if (at >= s.size()) return {};
  if (s[at] == '"') {
    const size_t from = ++at;
    while (at < s.size() && s[at] != '"') at++;
    const std::string_view w = s.substr(from, at - from);
    if (at < s.size()) at++;
    return w;
  }
  const size_t from = at;
  while (at < s.size() && !space(s[at])) at++;
  return s.substr(from, at - from);
}

std::string rest(std::string_view s, size_t at) {
  while (at < s.size() && space(s[at])) at++;
  size_t end = s.size();
  while (end > at && space(s[end - 1])) end--;
  std::string_view r = s.substr(at, end - at);
  if (r.size() >= 2 && r.front() == '"' && r.back() == '"') r = r.substr(1, r.size() - 2);
  return std::string(r);
}

std::string pad(std::string s, size_t n) {
  if (s.size() < n) s.resize(n, ' ');
  return s;
}

}  // namespace

bool setting_ban_matches(std::string_view match, std::string_view value) {
  if (!match.empty() && match.front() == '!') return !setting_ban_matches(match.substr(1), value);
  if (match == "*") return !value.empty();
  if (match.empty()) return value.empty();
  const std::string_view arg = match.substr(1);
  switch (match.front()) {
    case '=':
      return number(value) == number(arg);
    case '<':
      return number(value) < number(arg);
    case '>':
      return number(value) > number(arg);
    case '~':
      return contains(value, arg);
    case '#':
      return same(value, arg);
    default:
      return same(value, match);
  }
}

std::string setting_ban_query(std::string_view name) {
  return "cmd " + std::string(kSettingAnswer) + " " + std::string(name) + " $" + std::string(name) +
         "\n";
}

std::vector<const SettingBan*> setting_ban_hits(const SettingBans& bans, std::string_view name,
                                                std::string_view value) {
  std::vector<const SettingBan*> out;
  for (const SettingBan& b : bans.entries) {
    if (!same(b.name, name) || !setting_ban_matches(b.match, value)) continue;
    out.push_back(&b);
    if (b.action == FilterDo::kick) break;
  }
  return out;
}

bool setting_ban_command(SettingBans& bans, std::string_view kind, std::string_view line,
                         std::string& out, const char* const (*extra)[3]) {
  size_t at = 0;
  std::string word(next_word(line, at));
  for (char& ch : word) ch = low(ch);
  const std::string k(kind);
  const std::string ours[3] = {k + "_add", k + "_remove", k + "_list"};
  for (int i = 0; extra != nullptr && i < 3; i++)
    if (word == (*extra)[i]) word = ours[i];
  auto& e = bans.entries;
  const std::string what = k == "cvarban" ? "setting" : "key";
  if (word == ours[2]) {
    out.clear();
    if (e.empty()) {
      out = "No " + k + "s.\n";
      return true;
    }
    out = "id " + pad(what, 16) + " " + pad("match", 16) + " " + pad("action", 6) + " text\n";
    for (size_t i = 0; i < e.size(); i++)
      out += pad(std::to_string(i + 1), 2) + " " + pad(e[i].name, 16) + " " + pad(e[i].match, 16) +
             " " + pad(filter_do_name(e[i].action), 6) + " " + e[i].text + "\n";
    return true;
  }
  if (word == ours[1]) {
    const std::string arg(next_word(line, at));
    if (arg.empty()) {
      out = "usage: " + ours[1] + " <number|" + what + "|all>\n";
      return true;
    }
    if (e.empty()) {
      out = "No " + k + "s.\n";
      return true;
    }
    if (arg == "all") {
      out = "Removed " + std::to_string(e.size()) + " " + k + (e.size() == 1 ? "" : "s") + ".\n";
      e.clear();
      return true;
    }
    if (std::all_of(arg.begin(), arg.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
      const size_t n = static_cast<size_t>(std::strtoul(arg.c_str(), nullptr, 10));
      if (n < 1 || n > e.size()) {
        out = "There is no " + k + " " + arg + "; " + ours[2] + " numbers them.\n";
        return true;
      }
      out = "Removed " + k + " " + arg + " (" + e[n - 1].name + ").\n";
      e.erase(e.begin() + static_cast<std::ptrdiff_t>(n - 1));
      return true;
    }
    const size_t before = e.size();
    std::erase_if(e, [&](const SettingBan& b) { return same(b.name, arg); });
    const size_t n = before - e.size();
    out = n == 0 ? "No " + k + " names " + arg + ".\n"
                 : "Removed " + std::to_string(n) + " " + k + (n == 1 ? "" : "s") + ".\n";
    return true;
  }
  if (word != ours[0]) return false;
  SettingBan b;
  b.name = std::string(next_word(line, at));
  b.match = std::string(next_word(line, at));
  const std::string usage =
      "usage: " + ours[0] + " <" + what + "> <match> [log|print|stuff|kick] [text]\n";
  if (b.name.empty() || b.match.empty()) {
    out = usage;
    return true;
  }
  size_t after = at;
  std::string action(next_word(line, after));
  for (char& ch : action) ch = low(ch);
  if (!action.empty()) {
    const FilterDo kinds[4] = {FilterDo::log, FilterDo::print, FilterDo::stuff, FilterDo::kick};
    bool found = false;
    for (FilterDo d : kinds)
      if (action == filter_do_name(d)) {
        b.action = d;
        found = true;
      }
    if (!found) {
      out = ours[0] + ": " + action + " is not an action; give log, print, stuff or kick\n";
      return true;
    }
    at = after;
  }
  b.text = rest(line, at);
  for (const SettingBan& x : e)
    if (same(x.name, b.name) && same(x.match, b.match) && x.action == b.action) {
      out = ours[0] + ": " + b.name + " " + b.match + " " + filter_do_name(b.action) +
            " is listed already\n";
      return true;
    }
  e.push_back(b);
  out = "Added " + k + " " + std::to_string(e.size()) + ": " + b.name + " " + b.match + " " +
        filter_do_name(b.action) + "\n";
  return true;
}

std::string setting_ban_log_line(std::string_view player, std::string_view address,
                                 std::string_view name, std::string_view value) {
  return std::string(player) + "[" + std::string(address) + "]: matched cvarban: \"" +
         std::string(name) + "\" is \"" + std::string(value) + "\"";
}

}  // namespace ge::server
