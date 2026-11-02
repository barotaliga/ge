// The check of a command line against the catalog's rows (commands.cpp): every flag known to the
// command or global, a switch given no value and a valued flag its value, the number of words in
// range, and the words after `--` only for the commands that pass them on.
//
// ge:summary The check of a ge command line against its catalog row: flags, values, word counts
// ge:keywords cli ge check_args flags switch value words passthrough usage-error
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#include <algorithm>
#include <iterator>

#include "cli/commands.hpp"

namespace ge::cli {

namespace {

const FlagSpec* find_flag(const CommandSpec& c, std::string_view name) {
  for (const FlagSpec& f : c.flags) {
    if (f.name == name) return &f;
  }
  for (const FlagSpec& f : global_flags()) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

}  // namespace

std::optional<std::string> check_args(const Args& a) {
  if (!a.command) return "no such command";
  const CommandSpec& c = *a.command;
  for (const auto& [name, value] : a.flags) {
    const FlagSpec* f = find_flag(c, name);
    if (!f) {
      std::string why = "unknown flag --" + name;
      why += " for ge ";
      why += c.name;
      return why;
    }
    if (f->value.empty() && !value.empty()) return "--" + name + " is a switch and takes no value";
    if (!f->value.empty() && f->value[0] != '[' && value.empty()) {
      std::string why = "--" + name + " needs a value: --";
      why += name;
      why += '=';
      why += f->value;
      return why;
    }
  }
  const int n = static_cast<int>(a.words.size());
  if (n < c.min_args || (c.max_args >= 0 && n > c.max_args)) {
    return "ge " + std::string(c.name) + " takes " +
           (c.args.empty() ? std::string("no arguments") : std::string(c.args)) + ", got " +
           std::to_string(n) + " word(s)";
  }
  static constexpr std::string_view kPassing[] = {
      "lab",  "lab before", "lab diff", "lab pair", "lab sweep", "tool",  "play", "sim",
      "shot", "record",     "bench",    "profile",  "mem",       "leaks", "load"};
  if (!a.passthrough.empty() &&
      std::find(std::begin(kPassing), std::end(kPassing), c.name) == std::end(kPassing)) {
    return "only ge lab, lab before, lab diff, lab pair, lab sweep, tool, play, sim, shot, record, "
           "bench, profile, "
           "mem, "
           "leaks and load pass words after --";
  }
  return std::nullopt;
}

}  // namespace ge::cli
