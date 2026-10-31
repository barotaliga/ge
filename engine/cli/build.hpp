// THE INNER LOOP'S COMMANDS: `ge build`, `ge test`, `ge check` and `ge lab`, run on a game repo
// through its CMake presets (debug: sanitizers on; release) so an agent never types cmake, ctest
// or make. Each one writes the tools' full output to a log under build/<preset>/ and answers with
// findings instead: a compiler error as its file:line and message, a CMake error as its file and
// text, a failed test as its name and its harness's FAIL lines, a lab's FAIL lines as they are.
// A clean run answers in one line, so the output an agent reads stays small.
//
//   ge build   cmake --preset <p> [-DFETCHCONTENT_SOURCE_DIR_GAMEENGINE2=<engine>], cmake --build
//   ge test    the build, then ctest --preset <p> [-R <filter>] (the tests and the labs' smoke);
//              --affected: only the tests the change since --base reaches, only their targets built
//   ge check   format (clang-format --dry-run), ge doctor, the build, the tests: the whole gate
//   ge lab     the build, then build/<p>/labs: the list as JSON, or one lab with its report.json
//   ge lab diff  the lab's pictures against a kept run's, per distance band (diff.json)
//
// ge:summary ge build, test, check and lab: CMake, CTest and the labs run, their output as findings
// ge:keywords cli ge build test check lab cmake ctest clang-format compiler-errors findings log
// ge:keywords run_build run_test run_check run_lab compiler_findings test_findings preset
// ge:skill agent-interface
// ge:test tests/cli_build_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

struct BuildOptions {
  fs::path repo;
  std::string preset = "debug";
  std::string engine;  // --engine: a local engine tree the build uses instead of the download
};

Outcome run_build(const BuildOptions& o);
// With `affected` (a git ref), only the tests the change since it reaches (cli/reach.hpp).
// `list` with `affected`: name them and run nothing.
Outcome run_test(const BuildOptions& o, const std::string& filter, const std::string& affected = "",
                 bool list = false);
Outcome run_check(const BuildOptions& o);

struct LabOptions {
  std::string id;  // "" lists the labs
  bool smoke = false;
  std::string tag, compare;
  std::string shots;                     // "" every shot; "a,b" only those (the runner's --shots)
  std::vector<std::string> passthrough;  // the lab's own flags
};
Outcome run_lab(const BuildOptions& o, const LabOptions& lab);

// `ge lab diff <id>`: every picture of the lab's last run (or, with `run`, a run now) against
// the run kept with --tag=<against>, through the labs' own diff (engine/lab/diff.hpp); each band
// under its floor is an error finding pointing at its sheet.
struct LabDiffOptions {
  std::string id;
  std::string against = "before";
  bool run = false;
  std::string bands, psnr;               // "" keeps the labs' defaults
  std::vector<std::string> passthrough;  // the lab's own flags, with `run`
};
Outcome run_lab_diff(const BuildOptions& o, const LabDiffOptions& diff);

// The build `ge build` runs, for a command that needs it first: false with its findings in `out`.
bool build_into(const BuildOptions& o, Outcome& out);
// The same, building only `targets` (space-separated; the whole tree when one is unknown).
bool build_targets(const BuildOptions& o, Outcome& out, const std::string& targets);
// build/<preset>/<name>.log
fs::path build_log(const BuildOptions& o, std::string_view name);

struct Logged {
  int code;            // the exit code (-1 when it could not run)
  std::string output;  // stdout and stderr together
};
// Runs a command with its output appended to `log` and handed back.
Logged run_to_log(const std::string& command, const fs::path& log);

// Runs a command with its output thrown away; its exit code (-1 when it could not run).
int run_quiet(const std::string& command);
// Runs a command on this process's own output; its exit code (-1 when it could not run).
int run_passthrough(const std::string& command);

// The findings in a compiler's, a linker's or CMake's output (GCC, Clang, MSVC), at most `most`
// of them, each `where` relative to `repo` when it lies inside. Exposed for the tests.
std::vector<Finding> compiler_findings(const fs::path& repo, std::string_view output, int most);
// The findings in CTest's output: each failed test, and its harness's `file:line: FAIL` lines.
std::vector<Finding> test_findings(const fs::path& repo, std::string_view output,
                                   const std::string& preset);

}  // namespace ge::cli
