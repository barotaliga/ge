// The command line split into a command, its words, its flags and the words after `--`, and the
// flags read back by name. The catalog the split looks commands up in is commands.cpp.
//
// ge:summary The ge command line split into command, words, flags and passthrough, and flag lookups
// ge:keywords cli ge args parse_args has_flag flag_value passthrough command-line
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#include <algorithm>

#include "cli/commands.hpp"

namespace ge::cli {

Args parse_args(int argc, const char* const* argv) {
  Args a;
  bool rest = false;
  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (rest) {
      a.passthrough.push_back(arg);
    } else if (arg == "--") {
      rest = true;
    } else if (arg.starts_with("--")) {
      const size_t eq = arg.find('=');
      a.flags.emplace_back(arg.substr(2, eq == std::string::npos ? std::string::npos : eq - 2),
                           eq == std::string::npos ? "" : arg.substr(eq + 1));
    } else {
      a.words.push_back(arg);
    }
  }
  // The longest command name the leading words spell: "skills sync" before "skills".
  if (a.words.size() >= 2) a.command = find_command(a.words[0] + " " + a.words[1]);
  if (a.command) {
    a.words.erase(a.words.begin(), a.words.begin() + 2);
  } else if (!a.words.empty()) {
    a.command = find_command(a.words[0]);
    if (a.command) a.words.erase(a.words.begin());
  }
  return a;
}

bool has_flag(const Args& a, std::string_view name) {
  return std::any_of(a.flags.begin(), a.flags.end(),
                     [&](const auto& f) { return f.first == name; });
}

std::string flag_value(const Args& a, std::string_view name, std::string_view fallback) {
  for (const auto& [n, v] : a.flags) {
    if (n == name) return v;
  }
  return std::string(fallback);
}

}  // namespace ge::cli
