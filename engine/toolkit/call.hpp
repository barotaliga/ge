// THE TOOLKIT'S COMMANDS: the bodies of the commands a modder and a player share, written once and
// run by two front doors, `ge` (engine/cli: the developer's) and every game's player CLI
// (engine/gamecli: `secli` for Shooter's Edge), so a player who downloads a game without the engine
// has the same commands and no command is written twice.
//
// A GROUP is rows of data (`Row`: the words, the arguments, the flags, a summary, an example) and
// one run function each. A front door lists the rows in its help and catalog, checks the flags
// against them, and hands a matched command its `Call`: the program's name ("ge", "secli"), the
// positional words and the flags. The run answers an `Outcome` (outcome.hpp) the front door
// prints, its `next` lines spelled with the call's own program name.
//
// The groups (`rows()`): `pak` (pak.hpp: list, extract, build, convert and sum content archives)
// and `qc` (qc.hpp: QC built, disassembled, run, traced and profiled).
//
// A TOOL is a body the engine does not link: a row that needs one names it (`Row::tool`, "qc") and
// runs it through `Call::tool`, which each front door fills its own way. `ge` builds the game
// repo's tool executable and hands it the words; a game's CLI that links the plugin runs it in its
// own process. A front door lists only the rows whose tool it can run.
//
// ge:summary The toolkit's command rows and calls: one body for ge and every game's player CLI
// ge:keywords toolkit command-row call group shared-command modder player-cli gamecli ge
// ge:keywords run_row find_row has_flag flag_value program-name front-door
// ge:skill agent-interface
// ge:test tests/toolkit_pak_test.cpp
#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "toolkit/outcome.hpp"

namespace ge::toolkit {

// A flag a command reads; every flag is `--name=value` or `--name` (a switch: `value` "").
struct Flag {
  std::string_view name;   // without the dashes
  std::string_view value;  // the value's placeholder ("<dir>"), or "" for a switch
  std::string_view help;
};

// What a tool answered: its exit code and its output. `findings` with `code` < 0: the tool could
// not be run at all (not built, not linked), and the findings say why.
struct ToolRun {
  int code = 0;
  std::string output;
  std::string log;  // where the output was kept ("": nowhere, it ran in this process)
  std::vector<Finding> findings;
};

// Runs the tool `name` with `args` (its command, then its words and flags) for `program`, the name
// the tool's next steps are spelled with.
using ToolRunner = std::function<ToolRun(std::string_view program, std::string_view name,
                                         const std::vector<std::string>& args)>;

// A command's call, as a front door parsed it.
struct Call {
  std::string program = "ge";      // the program's name the answers spell their next steps with
  std::vector<std::string> words;  // positional, after the command's words
  std::vector<std::pair<std::string, std::string>> flags;  // --name=value; "" for a switch
  ToolRunner tool;  // runs a row's tool (`Row::tool`); empty: this front door runs none
};

bool has_flag(const Call& call, std::string_view name);
// The flag's value, or `fallback` when it is absent.
std::string flag_value(const Call& call, std::string_view name, std::string_view fallback = "");

struct Row {
  std::string_view name;   // the command's words: "pak ls"
  std::string_view args;   // its positional arguments as typed: "<archive> [<file>...]"
  int min_args, max_args;  // how many positional words it takes; -1: any number
  std::string_view stage;  // learn, create, build, see, maintain: where in an agent's loop
  std::string_view summary;
  std::span<const Flag> flags;
  std::string_view example;  // a whole command line after the program's name: "pak ls pak0.pak"
  std::string_view skill;    // the consumer skill that says how to use it
  Outcome (*run)(const Call& call);
  std::string_view tool = {};  // the tool its body hands the command to ("qc"), or "" for none
};

// Every group's rows, in the order a help lists them.
std::span<const Row> rows();
// The row named by these words ("pak ls"), or null.
const Row* find_row(std::string_view name);
// The row run: its word count checked (a usage error, exit 2, when wrong), then its body.
Outcome run_row(const Row& row, const Call& call);

}  // namespace ge::toolkit
