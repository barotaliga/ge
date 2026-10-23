// `ge ci`'s log reader (engine/cli/ci.hpp), the part that needs no network: a CI job's log, with
// GitHub's prefixes on each line, read back as findings: ge's own error lines first, then a
// compiler's and the tests' lines, then the log's last words.
//
// ge:summary Test: ge ci's job log as findings, and why a green pull request is blocked
// ge:keywords test cli ci log findings gh timestamp ci_log_findings last-words
// ge:keywords blocked_findings ruleset
// ge:skill testing agent-interface
#include <string>

#include "check.hpp"
#include "cli/ci.hpp"

using namespace ge::cli;

namespace {

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

const ge::cli::fs::path kRepo = "/work/game";

}  // namespace

TEST(ge_error_lines_are_read_through_the_tab_prefixes) {
  const std::string log =
      "build (debug)\tRun ge precheck\t2026-10-22T13:44:36.9046158Z ge check: note: built\n"
      "build (debug)\tRun ge precheck\t2026-10-22T13:44:37.0000000Z ge check: error: "
      "src/a.cpp is not formatted\n"
      "build (debug)\tRun ge precheck\t2026-10-22T13:44:37.0000001Z     where: src/a.cpp\n"
      "build (debug)\tRun ge precheck\t2026-10-22T13:44:37.0000002Z     next: ge fmt\r\n";
  const auto f = ci_log_findings(kRepo, "build (debug)", log);
  CHECK(f.size() == 1);
  CHECK(f[0].severity == Severity::error);
  CHECK(f[0].what == "src/a.cpp is not formatted");
  CHECK(f[0].where == "build (debug): src/a.cpp");
  CHECK(f[0].next == "ge fmt");
}

TEST(a_raw_job_log_drops_the_timestamp_and_the_bom) {
  const std::string log =
      "\xEF\xBB\xBF"
      "2026-10-22T13:44:36.9046158Z ##[group]Run ge precheck\n"
      "2026-10-22T13:44:37.1Z ge precheck: warn: lab sim's digest moved\n";
  const auto f = ci_log_findings(kRepo, "", log);
  CHECK(f.size() == 1 && f[0].severity == Severity::warn);
  CHECK(f[0].what == "lab sim's digest moved");
  CHECK(f[0].where == "");
  CHECK(has(f[0].next, "ge precheck"));
}

TEST(a_compiler_error_is_found_when_ge_did_not_run_it) {
  const std::string log =
      "2026-10-22T13:44:36.9Z [ 50%] Building CXX object a.o\n"
      "2026-10-22T13:44:37.0Z /work/game/src/a.cpp:12:5: error: 'x' was not declared\n"
      "2026-10-22T13:44:37.1Z ##[error]Process completed with exit code 2.\n";
  const auto f = ci_log_findings(kRepo, "build", log);
  CHECK(!f.empty());
  CHECK(has(f[0].what, "'x' was not declared"));
  CHECK(has(f[0].where, "build: "));
  CHECK(has(f[0].where, "src/a.cpp:12"));
}

TEST(a_log_naming_no_cause_reports_its_last_words) {
  const std::string log =
      "2026-10-22T13:44:36.9Z ##[group]Run ./tools/thing.sh\n"
      "2026-10-22T13:44:37.0Z the thing went sideways\n"
      "2026-10-22T13:44:37.1Z ##[error]Process completed with exit code 1.\n"
      "2026-10-22T13:44:37.2Z Post job cleanup.\n"
      "2026-10-22T13:44:37.3Z Cleaning up orphan processes\n";
  const auto f = ci_log_findings(kRepo, "job", log);
  CHECK(f.size() == 1 && f[0].severity == Severity::error);
  CHECK(f[0].what == "the job failed; its log ends: the thing went sideways");
  CHECK(f[0].where == "job: its log");

  const auto empty = ci_log_findings(kRepo, "", "");
  CHECK(empty.size() == 1 && has(empty[0].what, "names no cause"));
}

TEST(a_blocked_merge_names_the_required_check_that_never_passed) {
  const std::string rules =
      R"j([{"type":"deletion"},{"type":"required_status_checks","parameters":{)j"
      R"j("strict_required_status_checks_policy":false,"required_status_checks":[)j"
      R"j({"context":"lint","integration_id":1},{"context":"package (windows-x64)"}]}},)j"
      R"j({"type":"pull_request","parameters":{"required_approving_review_count":0}}])j";
  const auto f = blocked_findings("main", rules, {"lint", "fmt"});
  CHECK(f.size() == 1 && f[0].severity == Severity::error);
  CHECK(has(f[0].what, "\"package (windows-x64)\", which never passed"));
  CHECK(has(f[0].next, "removes it from the ruleset"));

  const std::string review_rule =
      R"j([{"type":"pull_request","parameters":{"required_approving_review_count":1}}])j";
  const auto review = blocked_findings("main", review_rule, {});
  CHECK(review.size() == 1 && has(review[0].what, "require 1 approving review"));
  CHECK(has(blocked_findings("main", "", {})[0].what, "could not be read"));
  CHECK(has(blocked_findings("main", "[]", {})[0].what, "rules do not name"));
}

TEST(a_moved_repository_is_named_with_the_remote_to_set) {
  Finding f;
  CHECK(moved_repo_finding("gh: Not Found (HTTP 404)\n", "a/b", "", f) == false);
  CHECK(moved_repo_finding("error: GET https://api.github.com/repositories/12/pulls/3: 403\n",
                           "a/b", "c/d", f));
  CHECK(f.what ==
        "the repository a/b has moved to c/d, and its old address answers with a "
        "redirect");
  CHECK(f.next == "git remote set-url origin https://github.com/c/d, then ge ci");
  CHECK(moved_repo_finding("HTTP 301: Moved Permanently", "a/b", "", f));
  CHECK(f.next.starts_with("set the origin remote"));
}

TEST(a_direct_merge_refused_for_a_merge_queue_is_told_apart) {
  CHECK(merge_queue_refusal("gh: Changes must be made through the merge queue (HTTP 405)"));
  CHECK(merge_queue_refusal("Repository rule violations found: Merge Queue required"));
  CHECK(!merge_queue_refusal("gh: Head branch was modified. Review and try the merge again."));
}

TEST(a_ci_answer_settles_on_an_error_or_when_nothing_runs) {
  Outcome running;
  running.summary = std::string(kCiPending);
  CHECK(!ci_settled(running));
  running.findings.push_back({Severity::error, "pull request 9 conflicts with main", "", ""});
  CHECK(ci_settled(running));
  Outcome green;
  green.summary = "every check passed";
  CHECK(ci_settled(green));
}

TEST(a_moved_repository_s_new_name_is_read_off_its_web_redirect) {
  CHECK(moved_to_of("HTTP/1.1 200 Connection Established\r\n\r\nHTTP/1.1 301 Moved "
                    "Permanently\r\nLocation: https://github.com/c/d\r\n") == "c/d");
  CHECK(moved_to_of("HTTP/2 200\r\ncontent-type: text/html\r\n").empty());
}
