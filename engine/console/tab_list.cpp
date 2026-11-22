// The definitions behind tab_list.hpp.
//
// ge:summary The listing Tab's definitions
// ge:keywords tab completion columns
// ge:skill console
// ge:test tests/console_view_test.cpp
#include "console/tab_list.hpp"

#include <algorithm>

namespace ge::console {

namespace {

void columns(const std::vector<std::string>& names, size_t width, std::vector<std::string>& out) {
  size_t widest = 1;
  for (const std::string& s : names) widest = std::max(widest, s.size());
  const size_t per = std::clamp<size_t>(width / (widest + 2), 1, 6);
  std::string row;
  for (size_t i = 0; i < names.size(); ++i) {
    std::string cell = names[i];
    if ((i + 1) % per != 0 && i + 1 < names.size()) cell.resize(widest + 2, ' ');
    row += cell;
    if ((i + 1) % per == 0 || i + 1 == names.size()) {
      out.push_back(row);
      row.clear();
    }
  }
}

}  // namespace

TabList tab_list(const Console& con, std::string_view input, std::string_view head, int mode,
                 int threshold, std::string& asked, size_t width) {
  TabList t;
  t.input = std::string(input);
  const bool slash = !input.empty() && (input[0] == '/' || input[0] == '\\');
  const std::string lead = slash ? std::string(input.substr(0, 1)) : std::string();
  if (head.empty()) return t;
  const std::vector<std::string> found = con.complete(head);
  if (found.empty()) return t;
  if (found.size() == 1) {
    t.input = lead + found[0] + " ";
    asked.clear();
    return t;
  }
  size_t same = found[0].size();  // what every match shares, the word completed to it
  for (const std::string& n : found) {
    size_t k = 0;
    while (k < same && k < n.size() && n[k] == found[0][k]) ++k;
    same = k;
  }
  if (same > head.size()) t.input = lead + found[0].substr(0, same);
  t.echo = "]" + t.input;
  const size_t n = found.size();
  if (static_cast<long long>(n) > threshold && asked != t.input) {
    asked = t.input;
    t.lines.push_back("Press TAB again to display all " + std::to_string(n) + " possibilities.");
    return t;
  }
  asked.clear();
  if (mode == 0) {
    t.lines = found;
  } else if (mode == 1) {
    columns(found, width, t.lines);
  } else {
    std::vector<std::string> commands, settings, aliases;
    for (const std::string& name : found)
      (con.cvars().find(name) != nullptr ? settings
       : con.aliases().count(name) != 0  ? aliases
                                         : commands)
          .push_back(name);
    const auto group = [&](const std::vector<std::string>& names, const char* one,
                           const char* many) {
      if (names.empty()) return;
      t.lines.push_back(std::to_string(names.size()) + " possible " +
                        (names.size() == 1 ? one : many) + ":");
      columns(names, width, t.lines);
    };
    group(commands, "command", "commands");
    group(settings, "variable", "variables");
    group(aliases, "alias", "aliases");
  }
  return t;
}

}  // namespace ge::console
