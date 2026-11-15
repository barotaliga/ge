// THE `qc` GROUP: a QC program (a progs file) built from its sources, disassembled, run, traced
// and profiled headless, for a mod's author with either front door:
//
//   qc build <progs.src|file.qc> [--out=<file>]   compiled; every mistake at its line
//   qc disasm <progs> [--function=<name>]          the file's counts and its functions
//   qc run <progs> --function=<name> [--args=...]  the function's return value or its fault
//   qc trace <progs> --function=<name> [...]       every call with arguments and return
//   qc profile <progs> --function=<name> [...]     statements and calls per function
//
// The bodies are the progs plugin's `qc` tool (plugins/progs/tool.hpp), which the engine does not
// link, so each row names the tool "qc" and runs it through `Call::tool`: `ge` builds the game
// repo's `progs` executable and hands it the words; a game's CLI that links the plugin calls the
// tool in its own process. The tool answers with its text, then `@qc finding`, `@qc summary` and
// `@qc result` lines; `qc_answer` turns them into the Outcome both front doors print, its next
// steps spelled with the calling program's name.
//
// ge:summary The qc command group: QC built, a progs disassembled, run, traced and profiled
// ge:keywords toolkit qc progs build compile disasm run trace profile qc_answer qc_rows tool
// ge:keywords bytecode modder answer-lines
// ge:skill agent-interface progs
// ge:test tests/toolkit_qc_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "toolkit/call.hpp"

namespace ge::toolkit {

// The tool's answer as an Outcome: its text, its findings, summary and result; `command` ("build",
// "run", ...) picks the next step. No answer line at all is an error naming `run.log`.
Outcome qc_answer(const Call& call, const ToolRun& run, std::string_view command = "");

std::span<const Row> qc_rows();

}  // namespace ge::toolkit
