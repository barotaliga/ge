// `ge server` (engine/cli/server.hpp): the repo's dedicated binaries found in its CMake files, the
// arguments a run hands one, what it printed read back, and the sample game's real server
// (gesample) run headless twice to one digest, and a flag it refuses said back.
//
// ge:summary Tests ge server: binaries found, arguments, the run read back, runs of gesample
// ge:keywords test cli ge server server_binaries server_argv read_server_run gesample replay
// ge:skill agent-interface testing
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "cli/server.hpp"
#include "server/process.hpp"

namespace fs = std::filesystem;
namespace gc = ge::cli;

TEST(cli_server_finds_the_repos_dedicated_binaries) {
  const std::vector<std::string> names = gc::server_binaries(GE_SOURCE_DIR);
  bool sample = false, edge = false;
  for (const std::string& n : names) {
    sample = sample || n == "gesample";
    edge = edge || n == "sesv";
  }
  CHECK(sample && edge);
  const fs::path none = fs::temp_directory_path() / "ge_cli_server_none";
  fs::create_directories(none);
  CHECK(gc::server_binaries(none).empty());
  fs::remove_all(none);
}

TEST(cli_server_argv_is_offline_with_its_log_unless_a_port_or_a_replay) {
  gc::ServerOptions s;
  s.map = "dm4";
  s.bots = "3";
  s.passthrough = {"+set", "sv_seed", "7"};
  std::vector<std::string> a = gc::server_argv(s, "runs/x.inputs");
  CHECK(a.front() == "--offline" && a[1] == "--for=60");
  CHECK(a[2] == "--map=dm4" && a[3] == "--bots=3" && a[4] == "--log=runs/x.inputs");
  CHECK(a.back() == "7");
  s.port = "27660";
  s.seconds = "5";
  a = gc::server_argv(s, "");
  CHECK(a.front() == "--port=27660" && a[1] == "--for=5");
  gc::ServerOptions r;
  r.replay = "runs/x.inputs";
  a = gc::server_argv(r, "ignored");
  CHECK(a.size() == 1 && a[0] == "--replay=runs/x.inputs");
}

TEST(cli_server_reads_a_run_and_a_replay_back) {
  gc::ServerRun run;
  CHECK(gc::read_server_run(
      "t=5.0 s frame=600 seated=2\n"
      "done: 7200 frames, server time 60.000 s, wall 0.31 s, digest 0123abcd45678901\n"
      "input log: runs/gesample.inputs (812 bytes); replay it with --replay=runs/gesample.inputs\n",
      run));
  CHECK(run.frames == "7200" && run.digest == "0123abcd45678901");
  CHECK(run.log == "runs/gesample.inputs");
  gc::ServerRun bad;
  CHECK(gc::read_server_run("replay: x leaves the log at frame 12 of 7200; compare\n", bad));
  CHECK(bad.diverged == "12" && bad.frames == "7200");
  gc::ServerRun none;
  CHECK(!gc::read_server_run("sample_server: unknown flag '--x'\n", none));
}

TEST(cli_server_runs_the_sample_server_twice_to_one_digest) {
  const fs::path dir = fs::path(ge::server::self_dir(nullptr)).parent_path();
  const fs::path exe = ge::server::program_path(dir.string(), "gesample");
  if (!fs::exists(exe)) {
    std::printf("skipped: no gesample beside the tests (make build makes it)\n");
    return;
  }
  const fs::path repo = fs::temp_directory_path() / "ge_cli_server_run";
  fs::remove_all(repo);
  fs::create_directories(repo);
  gc::ServerOptions s;
  s.seconds = "3";
  s.bots = "2";
  s.log = (repo / "runs" / "sample.inputs").string();
  const gc::Outcome run = gc::run_server_binary(repo, exe, s, repo / "server.log");
  CHECK_MSG(ge::toolkit::exit_of(run) == 0, run.findings.empty() ? run.text : run.findings[0].what);
  CHECK_MSG(run.summary.find("ran 180 frames, digest ") != std::string::npos, run.summary);
  // The sample game keeps no input log: said as a note, nothing to replay.
  CHECK(run.findings.size() == 1 && run.findings[0].severity == ge::toolkit::Severity::note);
  CHECK(run.next.empty());
  // The same run twice: the same digest.
  const gc::Outcome again = gc::run_server_binary(repo, exe, s, repo / "again.log");
  CHECK_MSG(again.summary == run.summary, again.summary + " vs " + run.summary);
  // A flag the server does not take comes back as an error with its words.
  gc::ServerOptions bad = s;
  bad.passthrough = {"--no-such-flag"};
  const gc::Outcome refused = gc::run_server_binary(repo, exe, bad, repo / "bad.log");
  CHECK(ge::toolkit::exit_of(refused) == 1);
  CHECK_MSG(!refused.findings.empty() &&
                refused.findings[0].what.find("unknown flag") != std::string::npos,
            refused.findings.empty() ? "" : refused.findings[0].what);
  fs::remove_all(repo);
}
