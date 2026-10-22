// THE CI LOOP'S COMMANDS: `ge precheck` (CI's gates, run here, before the push) and `ge ci` (why
// a CI job failed, as findings), so a red row costs one command instead of a CI round and a
// scroll through a log.
//
//   ge precheck [--preset=<p>]   what the repo's CI runs, in order, for each preset (both when
//                                none is named): `ge check` (format, doctor, build, tests and the
//                                labs against their baseline), then the pilot over seeds 1, 2, 3
//                                and every input log it wrote replayed to its digest. The game
//                                repo's own ci.yml runs `ge precheck --preset=<p>`, so the two
//                                are the same gate by construction.
//   ge ci [<pr>] [--log=<file>]  the failed checks of a pull request (default: the checkout's
//                                commit) through GitHub's `gh api`, each failed job's log read
//                                back as findings: ge's own error lines, compiler and CMake
//                                errors, test and lab FAIL lines, and the log's last words when
//                                none of those match. --log reads a saved job log instead (one
//                                downloaded by any other means), with no network.
//
// ge:summary ge precheck and ge ci: CI's gates run locally, and a failed CI job's log as findings
// ge:keywords cli ge precheck ci continuous-integration gates pull-request failed-job log gh
// ge:keywords run_precheck run_ci ci_log_findings github-actions
// ge:skill agent-interface
// ge:test tests/cli_ci_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

Outcome run_precheck(const BuildOptions& o, const std::vector<std::string>& presets);
Outcome run_ci(const fs::path& repo, const std::string& pr, const std::string& log_file);

// A CI job's log read back as findings, each `where` prefixed "<job>: " when `job` is not empty.
// A line may carry GitHub's prefixes (`gh run view --log-failed`'s "job<TAB>step<TAB>" and the
// timestamp before each line); they are dropped first.
std::vector<Finding> ci_log_findings(const fs::path& repo, std::string_view job,
                                     std::string_view log);

}  // namespace ge::cli
