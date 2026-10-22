// THE COMMAND CATALOG: every ge command stated once, as data. The parser checks a command line
// against it (an unknown flag or a wrong word count is a usage error), `ge help` prints it, and
// `ge help --json` hands it to an agent whole, so the help an agent reads cannot drift from what
// the binary accepts. A row names the command's words, its arguments, its flags, the stage of an
// agent's loop it serves, an example and the skill that owns it. `tests/cli_commands_test.cpp`
// holds every row to having all of them.
//
// Every command also takes `--json` (the answer as one JSON object, outcome.hpp) and `--help`.
// Everything after a bare `--` is passed through untouched (to a lab, by `ge lab`).
//
// ge:summary The ge command catalog as data: parsing, usage errors, ge help and ge help --json
// ge:keywords cli ge help catalog command-table flags usage parse args help_json help_text
// ge:keywords match_command check_args stage example discoverability
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::cli {

struct FlagSpec {
  std::string_view name;   // without the dashes
  std::string_view value;  // the value's placeholder ("<dir>"), or "" for a switch
  std::string_view help;
};

struct CommandSpec {
  std::string_view name;   // the command's words: "skills sync", "add lab"
  std::string_view args;   // its positional arguments as typed: "<dir>", "[<skill>]"
  int min_args, max_args;  // how many positional words it takes; -1: any number
  std::string_view stage;  // learn, create, build, see, maintain: where in an agent's loop
  std::string_view summary;
  std::span<const FlagSpec> flags;
  std::string_view example;
  std::string_view skill;  // the consumer skill that says how to use it
};

// Every command, in the order `ge help` lists them.
std::span<const CommandSpec> commands();

// A command line split up: the command's words matched, its positional words, its flags, and the
// words after a bare `--`.
struct Args {
  const CommandSpec* command = nullptr;
  std::vector<std::string> words;                          // positional, after the command
  std::vector<std::pair<std::string, std::string>> flags;  // --name=value; "" for a switch
  std::vector<std::string> passthrough;                    // after `--`
};

// Splits argv and matches the longest command name; `command` stays null when none matches
// (the first word is then in `words`).
Args parse_args(int argc, const char* const* argv);

// The usage error in `args`, if any: an unknown flag, a switch given a value, a flag missing its
// value, or a wrong number of words.
std::optional<std::string> check_args(const Args& args);

bool has_flag(const Args& args, std::string_view name);
// The flag's value, or `fallback` when it is absent.
std::string flag_value(const Args& args, std::string_view name, std::string_view fallback = "");

// One command's help, or the whole catalog's when `one` is null.
std::string help_text(const CommandSpec* one);
// The same as a JSON value: one command's object, or {"version","commands":[...],...}.
std::string help_json(const CommandSpec* one);

// The command named by these words ("add lab"), or null.
const CommandSpec* find_command(std::string_view name);

}  // namespace ge::cli
