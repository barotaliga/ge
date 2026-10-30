// THE `qc` TOOL: what `ge qc` runs (the executable `build/<preset>/progs`,
// plugins/progs/tool/main.cpp, is `qc_main` alone). Every command reads a progs file, checks it
// with the loader, and answers:
//
//   qc disasm <progs> [--function=<name>]   the file's counts, then every function (or one) as
//       statements with named operands (disasm.hpp)
//   qc run <progs> --function=<name> [--args=<n,...>] [--builtins=sample] [--entities=<n>]
//       the function run headless: its return value, the statements it ran, a fault's stack
//   qc trace <progs> --function=<name> [...run's flags] [--statements]
//       every call entered (with its arguments) and left (with its return value), indented by
//       depth, every builtin called; with --statements every statement too
//   qc profile <progs> --function=<name> [...run's flags] [--repeat=<n>] [--top=<n>]
//       the run repeated: each function's statements and calls, highest first, the builtins hit,
//       and the time it took (measured, so outside the answer's deterministic part)
//
// The arguments fill the function's parameters in order, a vector parameter taking three numbers.
// A progs's own builtins are its game's: none is installed unless `--builtins=sample` installs the
// sample program's (sample.hpp), so a call to one is the fault "builtin N is not provided". A
// fault is an error finding with its QC stack trace.
//
// THE ANSWER: the text body on the output, then protocol lines `ge qc` reads (engine/cli/qc.hpp):
// `@qc finding <severity>\t<what>\t<where>\t<next>`, `@qc summary <one line>`, `@qc result
// <json>`. The exit code: 0 done, 1 an error finding, 2 a usage error.
//
// ge:summary The qc tool behind ge qc: disasm, run, trace and profile a progs headless
// ge:keywords qc progs tool cli disasm run trace profile headless qc_main answer protocol
// ge:keywords ge-qc builtins-sample arguments
// ge:skill progs agent-interface
// ge:test tests/progs_tool_test.cpp
#pragma once

#include <ostream>

namespace ge::progs {

// The `qc` executable's main: argv[1] the command, then its words and flags.
int qc_main(int argc, const char* const* argv, std::ostream& out);

}  // namespace ge::progs
