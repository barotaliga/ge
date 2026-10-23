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
//   ge ci [<pr>] [--log=<file>] [--merge]
//                                the failed checks of a pull request (default: the checkout's
//                                commit) through GitHub's `gh api`, each failed job's log read
//                                back as findings: ge's own error lines, compiler and CMake
//                                errors, test and lab FAIL lines, and the log's last words when
//                                none of those match. --log reads a saved job log instead (one
//                                downloaded by any other means), with no network. --merge
//                                squash-merges the pull request when every check passed and it
//                                merges cleanly, pinned to the head the checks ran on. A
//                                pull request GitHub blocks with every check green is answered
//                                with why: the base's rules against the head's checks.
//
// ge:summary ge precheck and ge ci: CI's gates run locally, and a failed CI job's log as findings
// ge:keywords cli ge precheck ci continuous-integration gates pull-request failed-job log gh
// ge:keywords run_precheck run_ci ci_log_findings github-actions merge squash-merge
// ge:keywords blocked ruleset required-checks blocked_findings
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
Outcome run_ci(const fs::path& repo, const std::string& pr, const std::string& log_file,
               bool merge = false);
// `ge ci <pr> --wait[=<minutes>]`: run_ci once a minute until the pull request settles (a check
// failed, it conflicts, or every check finished), at most `minutes`; with `merge`, the settled
// green answer is then merged as --merge would. One blocking call where an agent would schedule
// check-ins: run it in the background and act on the answer when it exits.
Outcome run_ci_wait(const fs::path& repo, const std::string& pr, bool merge, int minutes);
// The summary run_ci gives while checks are still running.
inline constexpr std::string_view kCiPending = "nothing has failed yet";
// Whether a ge ci answer is final: an error, or no check still running.
bool ci_settled(const Outcome& o);
// Whether GitHub refused a direct merge because the base merges only through a merge queue
// (the refusal's text, any case).
bool merge_queue_refusal(std::string_view output);

// Why GitHub blocks a pull request whose checks all passed, off the base's rules (`gh api
// repos/<o>/<r>/rules/branches/<base>`, a JSON array; "" when they could not be read): each
// required check not among `passed` on the head, and a required review.
std::vector<Finding> blocked_findings(const std::string& base, std::string_view rules,
                                      const std::vector<std::string>& passed);

// A CI job's log read back as findings, each `where` prefixed "<job>: " when `job` is not empty.
// A line may carry GitHub's prefixes (`gh run view --log-failed`'s "job<TAB>step<TAB>" and the
// timestamp before each line); they are dropped first.
// The finding for a gh call that failed because the repository moved (GitHub answers the old
// name with a redirect to `repositories/<id>/...`, which a proxy may refuse): `moved_to` is the
// owner/repo GitHub now names it ("" when that was not found), `slug` the remote's. False when
// `error` is not a moved repository's.
bool moved_repo_finding(std::string_view error, const std::string& slug,
                        const std::string& moved_to, Finding& out);
std::vector<Finding> ci_log_findings(const fs::path& repo, std::string_view job,
                                     std::string_view log);

}  // namespace ge::cli
