// How `ge sim`, `ge replay`, `ge state` and `ge diverge` read the play executable's answer back
// (engine/cli/play.hpp): each JSON answer becomes the outcome's result with a short text, an
// error answer becomes a finding with its paths relative to the repo, a replay that misses its
// digest is an error, and a missing answer names the log. Running them for real on a scaffolded
// repo is the packaging workflow's `scaffold` job.
//
// ge:summary Tests: the play executable's JSON answers read back as ge outcomes and findings
// ge:keywords test cli ge play sim replay state diverge play_answer outcome
// ge:skill agent-interface testing
// ge:test tests/cli_play_test.cpp
#include <string>

#include "check.hpp"
#include "cli/json.hpp"
#include "cli/play.hpp"

using namespace ge::cli;

namespace {

BuildOptions repo() {
  BuildOptions o;
  o.repo = fs::temp_directory_path() / "ge_cli_play_test_repo";
  return o;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(a_sim_answer_lists_each_run_and_points_at_its_replay) {
  const std::string inputs = (repo().repo / "runs" / "g-38.inputs").string();
  const Outcome o =
      play_answer(repo(), "sim", 0,
                  "{\"game\":\"g\",\"runs\":[{\"seed\":38,\"inputs\":" + json_str(inputs) +
                      ",\"steps\":10,\"over\":true,\"digest\":\"00000000000000ff\","
                      "\"report\":{\"x\":1}}]}\n");
  CHECK(exit_of(o) == 0 && has(o.result, "\"runs\":["));
  CHECK_MSG(has(o.text, "seed 38: over after 10 steps, digest 00000000000000ff, runs/g-38.inputs"),
            o.text);
  CHECK(o.next == "ge replay runs/g-38.inputs");
}

TEST(a_replay_that_misses_its_digest_is_an_error) {
  const Outcome ok = play_answer(repo(), "replay", 0,
                                 "{\"inputs\":\"a\",\"recorded\":\"01\",\"matches\":true,"
                                 "\"steps\":3,\"over\":false,\"digest\":\"01\",\"report\":{}}");
  CHECK(exit_of(ok) == 0 && has(ok.summary, "to digest 01, the one recorded"));
  const Outcome bad = play_answer(repo(), "replay", 1,
                                  "{\"inputs\":\"a\",\"recorded\":\"01\",\"matches\":false,"
                                  "\"steps\":3,\"over\":false,\"digest\":\"02\",\"report\":{}}");
  CHECK(exit_of(bad) == 1 && bad.findings.size() == 1);
  CHECK(has(bad.findings[0].what, "02 differs from the recorded 01") &&
        has(bad.findings[0].next, "ge diverge"));
}

TEST(a_diverge_answer_says_where_the_runs_part) {
  const Outcome o = play_answer(
      repo(), "diverge", 1,
      "{\"same\":false,\"setup_differs\":false,\"input_step\":4,\"state_step\":5,"
      "\"fields\":[{\"key\":\"x\",\"a\":\"1\",\"b\":\"2\"}],\"more_fields\":0,\"steps_a\":9,"
      "\"steps_b\":9,\"digest_a\":\"01\",\"digest_b\":\"02\"}");
  CHECK(exit_of(o) == 0);
  CHECK_MSG(has(o.text, "inputs first differ at step 4") && has(o.text, "x: 1 vs 2"), o.text);
  CHECK(o.summary == "the runs part after step 5" &&
        o.next == "ge state <log> --step=5 for either side");
  const Outcome same = play_answer(repo(), "diverge", 0,
                                   "{\"same\":true,\"input_step\":-1,\"state_step\":-1,"
                                   "\"fields\":[],\"digest_a\":\"01\"}");
  CHECK(has(same.summary, "the same run"));
}

TEST(an_error_answer_becomes_a_finding_relative_to_the_repo) {
  const std::string path = (fs::weakly_canonical(repo().repo) / "runs" / "x.inputs").string();
  const Outcome o =
      play_answer(repo(), "replay", 1,
                  "warning from the game's stderr\n{\"error\":" + json_str("cannot read " + path) +
                      ",\"where\":" + json_str(path) + ",\"next\":\"pass an input log\"}");
  CHECK(o.findings.size() == 1);
  CHECK_MSG(
      o.findings[0].what == "cannot read runs/x.inputs" && o.findings[0].where == "runs/x.inputs",
      o.findings[0].what + " | " + o.findings[0].where);
  const Outcome usage =
      play_answer(repo(), "sim", 2, "{\"error\":\"x\",\"where\":\"y\",\"next\":\"z\"}");
  CHECK(exit_of(usage) == 2);
  const Outcome none = play_answer(repo(), "sim", 134, "Segmentation fault\n");
  CHECK(none.findings.size() == 1 && has(none.findings[0].what, "no JSON answer (exit 134)") &&
        has(none.findings[0].where, "ge-play.log"));
}
