// A limited remote console for helpers (lrcon.hpp states the rule).
//
// ge:summary lrcon: the allowed prefixes judged, added, taken away and listed
// ge:keywords lrcon lrcon_allows lrcon_command lrcon_add lrcon_del lrcon_list
// ge:skill net
// ge:test tests/server_lrcon_test.cpp
#include "server/lrcon.hpp"

#include <algorithm>

namespace ge::server {

bool lrcon_allows(const LimitedRcon& l, const std::string& line) {
  return std::any_of(l.prefixes.begin(), l.prefixes.end(), [&](const std::string& p) {
    return !p.empty() && line.compare(0, p.size(), p) == 0;
  });
}

bool lrcon_command(LimitedRcon& l, const std::string& line, std::string& out,
                   const char* const (*extra)[3]) {
  const size_t space = line.find(' ');
  const std::string cmd = line.substr(0, space);
  std::string arg = space == std::string::npos ? "" : line.substr(space + 1);
  while (!arg.empty() && (arg.back() == '\n' || arg.back() == '\r')) arg.pop_back();
  auto is = [&](const char* ours, int k) { return cmd == ours || (extra && cmd == (*extra)[k]); };
  if (is("lrcon_add", 0)) {
    if (arg.empty()) {
      out = "Usage: lrcon_add <the start of a line a helper may run>\n";
    } else if (std::find(l.prefixes.begin(), l.prefixes.end(), arg) == l.prefixes.end()) {
      l.prefixes.push_back(arg);
      out = "Helpers may now run \"" + arg + "\".\n";
    } else {
      out = "\"" + arg + "\" is already allowed.\n";
    }
    return true;
  }
  if (is("lrcon_del", 1)) {
    const auto it = std::find(l.prefixes.begin(), l.prefixes.end(), arg);
    if (it == l.prefixes.end()) {
      out = "\"" + arg + "\" is not on the list.\n";
    } else {
      l.prefixes.erase(it);
      out = "Helpers may no longer run \"" + arg + "\".\n";
    }
    return true;
  }
  if (is("lrcon_list", 2)) {
    out = l.prefixes.empty() ? "Helpers may run nothing.\n" : "";
    for (const std::string& p : l.prefixes) out += p + "\n";
    return true;
  }
  return false;
}

}  // namespace ge::server
