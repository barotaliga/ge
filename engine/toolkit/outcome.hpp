// WHAT A COMMAND ANSWERS, in `ge` and in every game's player CLI: one Outcome, printed as text for
// a reader or as one JSON object (`--json`) for an agent. Every command fills the same shape, so an
// agent learns it once (the program's name, "ge" or "secli", leads the command):
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

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace ge::toolkit {

enum class Severity : uint8_t {
  error,  // the work found a problem; the command exits 1
  warn,   // odd but allowed; worth a look
  note,   // information the next step needs
};

// A FINDING: one thing a command has to tell an agent, in the diagnostics' three parts (what
// failed, where, the next action; docs/architecture.md § Diagnostics).
struct Finding {
  Severity severity;
  std::string what;   // one line, with the values involved
  std::string where;  // a path (":line" when there is one), or "" for the repository
  std::string next;   // the command to run, the skill to load or the file to read
};

// "<command>: error: <what>\n    where: <where>\n    next: <next>\n"
std::string format_finding(std::string_view command, const Finding& f);
int count_of(const std::vector<Finding>& findings, Severity severity);

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

// The lines of `text`, split on '\n' (a trailing newline makes no empty last line).
std::vector<std::string_view> split_lines(std::string_view text);

}  // namespace ge::toolkit
