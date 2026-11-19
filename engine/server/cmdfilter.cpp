// Commands a server refuses from its clients (cmdfilter.hpp).
//
// ge:summary filtercmd_add's list: the match, the administrator's words, the log line
// ge:keywords filtercmd cmdfilter_match cmdfilter_command
// ge:skill net
// ge:test tests/server_cmdfilter_test.cpp
#include "server/cmdfilter.hpp"

#include <algorithm>
#include <cstdlib>

namespace ge::server {

namespace {

constexpr const char* kNames[] = {"ignore", "log", "print", "stuff", "kick"};

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& ch : out)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return out;
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

// The rest of `s` from `at`, its spaces and one pair of outer quotes trimmed.
std::string rest(std::string_view s, size_t at) {
  while (at < s.size() && space(s[at])) at++;
  size_t end = s.size();
  while (end > at && space(s[end - 1])) end--;
  std::string_view r = s.substr(at, end - at);
  if (r.size() >= 2 && r.front() == '"' && r.back() == '"') r = r.substr(1, r.size() - 2);
  return std::string(r);
}

std::string entry_line(size_t n, const CmdFilter& f) {
  std::string out = std::to_string(n) + " " + f.command + " " + filter_do_name(f.action);
  if (!f.text.empty()) out += " \"" + f.text + "\"";
  return out + "\n";
}

}  // namespace

const char* filter_do_name(FilterDo action) { return kNames[static_cast<int>(action)]; }

const CmdFilter* cmdfilter_match(const CmdFilters& filters, std::string_view line) {
  if (filters.entries.empty()) return nullptr;
  size_t at = 0;
  const std::string cmd = lower(next_word(line, at));
  if (cmd.empty()) return nullptr;
  for (const CmdFilter& f : filters.entries)
    if (f.command == cmd) return &f;
  return nullptr;
}

bool cmdfilter_command(CmdFilters& filters, std::string_view line, std::string& out,
                       const char* const (*extra)[3]) {
  size_t at = 0;
  std::string word = lower(next_word(line, at));
  constexpr const char* kOurs[3] = {"filtercmd_add", "filtercmd_remove", "filtercmd_list"};
  for (int k = 0; extra != nullptr && k < 3; k++)
    if (word == (*extra)[k]) word = kOurs[k];
  if (word != kOurs[0] && word != kOurs[1] && word != kOurs[2]) return false;
  if (word == "filtercmd_list") {
    out.clear();
    for (size_t i = 0; i < filters.entries.size(); i++)
      out += entry_line(i + 1, filters.entries[i]);
    out += std::to_string(filters.entries.size()) + " commands filtered\n";
    return true;
  }
  const std::string what = lower(next_word(line, at));
  if (what.empty()) {
    out = word == "filtercmd_add"
              ? "usage: filtercmd_add <command> [ignore|log|print|stuff|kick] [text]\n"
              : "usage: filtercmd_remove <command|number|all>\n";
    return true;
  }
  auto& e = filters.entries;
  if (word == "filtercmd_remove") {
    if (what == "all") {
      out = "removed " + std::to_string(e.size()) + " command filters\n";
      e.clear();
      return true;
    }
    char* end = nullptr;
    const long n = std::strtol(what.c_str(), &end, 10);
    if (end != what.c_str() && *end == '\0') {
      if (n < 1 || static_cast<size_t>(n) > e.size()) {
        out = "filtercmd_remove: there is no entry " + what + "; filtercmd_list numbers them\n";
        return true;
      }
      out = "removed " + e[static_cast<size_t>(n - 1)].command + "\n";
      e.erase(e.begin() + (n - 1));
      return true;
    }
    const auto it =
        std::find_if(e.begin(), e.end(), [&](const CmdFilter& f) { return f.command == what; });
    if (it == e.end()) {
      out = "filtercmd_remove: " + what + " is not filtered; filtercmd_list shows the list\n";
      return true;
    }
    e.erase(it);
    out = "removed " + what + "\n";
    return true;
  }
  CmdFilter f;
  f.command = what;
  size_t after = at;
  const std::string action = lower(next_word(line, after));
  if (!action.empty()) {
    int found = -1;
    for (int i = 0; i < 5; i++)
      if (action == kNames[i]) found = i;
    if (found < 0) {
      out = "filtercmd_add: " + action +
            " is not an action; give ignore, log, print, stuff or kick\n";
      return true;
    }
    f.action = static_cast<FilterDo>(found);
    at = after;
  }
  f.text = rest(line, at);
  if ((f.action == FilterDo::print || f.action == FilterDo::stuff) && f.text.empty()) {
    out = std::string("filtercmd_add: ") + kNames[static_cast<int>(f.action)] +
          " needs the text to " + (f.action == FilterDo::print ? "show" : "run") + "\n";
    return true;
  }
  std::erase_if(e, [&](const CmdFilter& x) { return x.command == f.command; });
  e.push_back(f);
  out = "added " + entry_line(e.size(), f);
  return true;
}

std::string cmdfilter_log_line(std::string_view name, std::string_view address,
                               std::string_view line) {
  size_t at = 0;
  return std::string(name) + "[" + std::string(address) +
         "]: issued banned command: " + std::string(next_word(line, at));
}

}  // namespace ge::server
