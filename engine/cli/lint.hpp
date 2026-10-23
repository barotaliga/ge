// `ge lint [<file>...]`: clang-tidy's bug-finding checks over what a change reaches, answered as
// findings. With no file it lints each .cpp changed since --base (default origin/main, the merge
// base of it and HEAD, untracked files included) and each .cpp whose object includes a changed
// header (Ninja's header dependencies, read with `ninja -t deps`); --all lints every .cpp. The
// checks are the repository's .clang-tidy (`ge new` writes one; a repo without one gets the same
// bug-finding set on the command line). Each finding is its file:line, the message and the check,
// with the check's documentation as the next step. It is the lint a change owes, early: a few
// files in seconds where the whole tree takes minutes.
//
//   ge lint                          what the change since origin/main reaches
//   ge lint --base=HEAD~3            ...since another ref
//   ge lint engine/x.cpp games/y.cpp exactly these files
//   ge lint --all                    every .cpp
//
// ge:summary ge lint: clang-tidy over the files a change reaches, its findings as findings
// ge:keywords cli ge lint clang-tidy bug-finding changed-files header-dependencies ninja-deps
// ge:keywords run_lint lint_findings tidy_check_url merge-base
// ge:skill agent-interface
// ge:test tests/cli_lint_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

struct LintOptions {
  std::vector<std::string> files;  // exactly these ("": what the change reaches)
  std::string base = "origin/main";
  bool all = false;
};
Outcome run_lint(const BuildOptions& o, const LintOptions& l);

// The checks a repository without a .clang-tidy is linted with: bug-finding only, as errors.
inline constexpr std::string_view kDefaultTidyChecks =
    "-*,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,"
    "performance-*,clang-analyzer-*";

// clang-tidy's diagnostics in `output` (`file:line:col: warning|error: message [check]`), each a
// finding at its repository-relative file:line, at most `most` (duplicates, which a header's
// finding is through every file that includes it, once).
std::vector<Finding> lint_findings(const fs::path& repo, std::string_view output, int most);
// The documentation page of a check (`bugprone-use-after-move`, `clang-analyzer-core.NullDeref`).
std::string tidy_check_url(std::string_view check);

}  // namespace ge::cli
