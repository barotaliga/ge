// Both forms of the help, read off the catalog (commands.cpp): `ge help` as text and `ge help
// --json` as one object, a command's usage line built from its flags.
//
// ge:summary ge help and ge help --json printed from the command catalog
// ge:keywords cli ge help help_text help_json usage_line command_json flag_json catalog
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#include "cli/commands.hpp"
#include "cli/embedded.hpp"
#include "cli/json.hpp"

namespace ge::cli {

namespace {

// A flag's value as help prints it: "=<dir>", "[=<minutes>]" for an optional one ("[<minutes>]"),
// or nothing for a switch.
std::string flag_value_text(std::string_view value) {
  if (value.empty()) return "";
  if (value[0] == '[') return "[=" + std::string(value.substr(1));
  return "=" + std::string(value);
}

std::string usage_line(const CommandSpec& c) {
  std::string line = "ge " + std::string(c.name);
  if (!c.args.empty()) line += " " + std::string(c.args);
  for (const FlagSpec& f : c.flags) {
    line += " [--" + std::string(f.name) + flag_value_text(f.value) + "]";
  }
  return line;
}

std::string flag_json(const FlagSpec& f) {
  return json_obj({{"name", json_str("--" + std::string(f.name))},
                   {"value", f.value.empty() ? "null" : json_str(f.value)},
                   {"help", json_str(f.help)}});
}

std::string command_json(const CommandSpec& c) {
  std::vector<std::string> flags;
  for (const FlagSpec& f : c.flags) flags.push_back(flag_json(f));
  return json_obj({{"name", json_str(c.name)},
                   {"usage", json_str(usage_line(c))},
                   {"args", json_str(c.args)},
                   {"stage", json_str(c.stage)},
                   {"summary", json_str(c.summary)},
                   {"flags", json_arr(flags)},
                   {"example", json_str(c.example)},
                   {"skill", json_str(c.skill)}});
}

}  // namespace

std::string help_text(const CommandSpec* one) {
  if (one) {
    std::string out = usage_line(*one) + "\n  " + std::string(one->summary) + "\n";
    for (const FlagSpec& f : one->flags) {
      out += "  --" + std::string(f.name) + flag_value_text(f.value) + "  " + std::string(f.help) +
             "\n";
    }
    out += "  --json  the answer as one JSON object\n  example: " + std::string(one->example) +
           "\n  skill: " + std::string(one->skill) + "\n";
    return out;
  }
  std::string out = "ge " + std::string(engine_version()) +
                    ": the engine's command line, the one interface an agent makes a game "
                    "through.\nEvery command takes --json (one JSON object: command, ok, exit, "
                    "findings, written, result, summary, next) and --help.\nExit codes: 0 done, 1 "
                    "the work found a problem, 2 a usage error.\n";
  std::string_view stage;
  for (const CommandSpec& c : commands()) {
    if (c.stage != stage) {
      stage = c.stage;
      out += "\n" + std::string(stage) + ":\n";
    }
    std::string head =
        "  ge " + std::string(c.name) + (c.args.empty() ? "" : " ") + std::string(c.args);
    if (head.size() < 34) head.resize(34, ' ');
    out += head + "  " + std::string(c.summary) + "\n";
  }
  out += "\n`ge help <command>` for its flags; `ge help --json` for the whole catalog.\n";
  return out;
}

std::string help_json(const CommandSpec* one) {
  if (one) return command_json(*one);
  std::vector<std::string> rows;
  for (const CommandSpec& c : commands()) rows.push_back(command_json(c));
  std::vector<std::string> global;
  for (const FlagSpec& f : global_flags()) global.push_back(flag_json(f));
  return json_obj({{"version", json_str(engine_version())},
                   {"exit_codes", json_obj({{"0", json_str("done")},
                                            {"1", json_str("the work found a problem")},
                                            {"2", json_str("a usage error")}})},
                   {"global_flags", json_arr(global)},
                   {"commands", json_arr(rows)}});
}

}  // namespace ge::cli
