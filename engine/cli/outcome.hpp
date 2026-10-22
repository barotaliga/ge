// WHAT A ge COMMAND ANSWERS: one Outcome, printed as text for a reader or as one JSON object
// (`--json`) for an agent. Every command fills the same shape, so an agent learns it once:
//
//   {"command":"ge build","ok":true,"exit":0,
//    "findings":[{"severity":"error","what":"...","where":"sim/game.cpp:12","next":"..."}],
//    "written":["sim/wind.hpp"],"result":{...},"summary":"...","next":"ge test"}
//
// `findings` are the diagnostics' three parts (finding.hpp); `written` every file the command
// changed, repository-relative; `result` the command's own answer (null when it has none);
// `summary` one line; `next` the command most likely wanted after this one. The exit code: 0 done,
// 1 the work found a problem (any error finding), 2 a usage error.
//
// ge:summary A ge command's answer (findings, files written, result, next) as text or JSON
// ge:keywords cli ge outcome result json exit-code summary next findings written emit
// ge:keywords finding_json machine-readable
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli/finding.hpp"

namespace ge::cli {

struct Outcome {
  std::vector<Finding> findings;
  std::vector<std::string> written;  // repository-relative paths
  std::string result;                // a JSON value, or "" for none
  std::string text;                  // what text mode prints before the findings (may be "")
  std::string summary;               // one line when no finding is an error
  std::string next;                  // the command most likely wanted next, or ""
  int exit = -1;                     // -1: 1 when any finding is an error, else 0
};

// The exit code the outcome stands for.
int exit_of(const Outcome& o);

// One finding as a JSON object.
std::string finding_json(const Finding& f);

// Prints the outcome for `command` ("ge build") and returns its exit code. Text mode: the files
// written, `text`, every finding, then the summary (or the error count) and the next command.
int emit(std::string_view command, const Outcome& o, bool json, std::ostream& out);

}  // namespace ge::cli
