// `ge qc`: a QC program (a progs file) built from its sources, disassembled, run, traced and
// profiled headless, by the repo's `qc` executable (built from the progs plugin's tool: the
// engine's command line never links a plugin, so it builds that target and hands the command
// to it):
//
//   ge qc build <progs.src|file.qc> [--out=<file>]   compiled; every mistake at its line
//   ge qc disasm <progs> [--function=<name>]          the file's counts and its functions
//   ge qc run <progs> --function=<name> [--args=...]  the function's return value or its fault
//   ge qc trace <progs> --function=<name> [...]       every call with arguments and return
//   ge qc profile <progs> --function=<name> [...]     statements and calls per function
//
// The executable answers with its text, then `@qc finding`, `@qc summary` and `@qc result` lines
// (plugins/progs/tool.hpp); `qc_answer` turns them into the Outcome every command prints.
//
// ge:summary ge qc: QC built; a progs disassembled, run, traced and profiled by the repo's tool
// ge:keywords cli ge qc progs build compile disasm run trace profile qc_answer run_qc bytecode
// ge:skill agent-interface progs
// ge:test tests/cli_qc_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

// The repo's build of the `qc` target, then `qc <command> <file> <flags...>`.
Outcome run_qc(const BuildOptions& o, const std::string& command, const std::string& file,
               const std::vector<std::string>& flags);
// The executable's output and exit code as an Outcome (exposed for the tests).
Outcome qc_answer(const BuildOptions& o, int code, const std::string& output,
                  const std::string& command = "");

}  // namespace ge::cli
