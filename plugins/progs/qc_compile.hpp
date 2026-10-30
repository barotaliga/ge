// THE QC COMPILER: source files in, a progs out (file.hpp's `Progs`, with its line table), or the
// diagnostic that stopped it. The language is the one a mod's game logic is written in (the
// `progs` skill, § The language): floats, vectors, strings, entities, fields and functions;
// frame functions; builtins by number; the classic declaration forms and the C forms beside them.
//
// One pass: every declaration is laid out and every function body emitted as it is read. Each
// function's parameters, locals and temporaries form one block of globals placed after the
// function is read (so the block is contiguous whatever constants the body added), which the VM
// saves on entry and restores on leave, so recursion keeps them.
//
// The program is stamped with the classic layout's crc (layout.hpp) only when its declarations
// put every engine-side global and field where the engine reads them; otherwise crc 0, which a
// game refuses at load, so a mod with a misplaced field never runs on shifted memory.
//
// A project file (`progs.src`) names the output and the sources in order; `compile_project`
// reads it and the sources from disk, `compile_qc` takes the texts. The output depends on the
// texts and the names alone, so a mod built twice is the same file byte for byte.
//
// ge:summary The QC compiler: sources to a progs and its line table, or the first error
// ge:keywords qc compiler compile qcc progs-src project bytecode codegen parser frame-function
// ge:keywords builtin diagnostic compile_qc compile_project mod baseline-mod qc-build
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "progs/file.hpp"

namespace ge::progs {

struct QcSource {
  std::string name;  // as written into the function records and the diagnostics
  std::string text;
};

struct QcDiagnostic {
  std::string file;
  int line = 0;
  bool error = true;  // false: a warning
  std::string message;
};

// "file:line: error: message" (or "warning:").
std::string diagnostic_text(const QcDiagnostic& d);

struct QcResult {
  bool ok = false;  // no errors (warnings allowed)
  Progs progs;      // version 6, its crc, `lines` one per statement
  std::vector<QcDiagnostic> diagnostics;
};

// Compiles the sources as one program, in order. Stops at the first error.
QcResult compile_qc(std::span<const QcSource> sources);

struct QcProject {
  QcResult result;
  std::filesystem::path output;  // where the project file says the progs goes
};

// Reads `project` (a progs.src: the output's name, then the sources, each relative to the
// project's directory; `//` comments) and compiles it. A missing file is a diagnostic.
QcProject compile_project(const std::filesystem::path& project);

}  // namespace ge::progs
