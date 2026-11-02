// How `ge build`, `ge test` and `ge check` read the tools' output back (engine/cli/build.hpp):
// GCC/Clang, MSVC, CMake, linker and clang-format errors become findings with their file and
// line, each next action names what to run, a repeated error is listed once and a flood is
// capped; CTest's failed tests and the harness's FAIL lines become findings too. Running the
// tools for real on a scaffolded repo is the packaging workflow's `scaffold` job.
//
// ge:summary Tests: compiler, CMake, linker, CTest and lab output read back as ge findings
// ge:keywords test cli ge build test check compiler_findings test_findings parse output
// ge:skill agent-interface testing
// ge:test tests/cli_build_test.cpp
#include <cstdlib>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/build.hpp"

using namespace ge::cli;

namespace {

const fs::path kRepo = fs::temp_directory_path() / "ge_cli_build_test_repo";

std::string in_repo(const std::string& rel) { return (kRepo / rel).generic_string(); }

}  // namespace

TEST(compiler_errors_become_findings_at_their_line) {
  const std::string out =
      "[1/4] Building CXX object CMakeFiles/x.dir/sim/wind.cpp.o\n"
      "FAILED: CMakeFiles/x.dir/sim/wind.cpp.o\n" +
      in_repo("sim/wind.cpp") + ":31:40: error: 'gust' was not declared in this scope\n" +
      in_repo("sim/wind.cpp") + ":31:40: error: 'gust' was not declared in this scope\n" +
      "C:\\g\\sim\\wind.cpp(12,5): error C2065: 'gust': undeclared identifier\n" +
      in_repo("sim/game.hpp") + ":4:10: fatal error: 'nope.hpp' file not found\n" +
      "/usr/bin/ld: game.cpp.o: undefined reference to `step_wind(Wind&)'\n";
  const auto f = compiler_findings(kRepo, out, 20);
  CHECK_MSG(f.size() == 4, std::to_string(f.size()) + " findings");
  if (f.size() != 4) return;
  CHECK(f[0].where == "sim/wind.cpp:31" && f[0].what.find("'gust'") != std::string::npos);
  CHECK(f[0].next.find("sim/wind.cpp:31") != std::string::npos);
  CHECK(f[1].what.starts_with("C2065: ") && f[1].where.ends_with(":12"));
  CHECK(f[2].where == "sim/game.hpp:4");
  CHECK(f[3].where == "the link" && f[3].next.find("ge use") != std::string::npos);
}

TEST(a_flood_of_errors_is_capped_with_a_note) {
  std::string out;
  for (int i = 1; i <= 30; i++)
    out += in_repo("a.cpp") + ":" + std::to_string(i) + ":1: error: x\n";
  const auto f = compiler_findings(kRepo, out, 20);
  CHECK(f.size() == 21);
  CHECK(f.back().severity == Severity::note && f.back().what.starts_with("10 more"));
}

TEST(cmake_and_format_errors_name_their_file_and_fix) {
  const std::string out =
      "CMake Error at CMakeLists.txt:34 (target_link_libraries):\n"
      "  Target \"x_sim\" links to:\n\n    ge::nope\n\n"
      "  but the target was not found.\n" +
      in_repo("sim/wind.cpp") +
      ":3:1: error: code should be clang-formatted "
      "[-Wclang-format-violations]\n";
  const auto f = compiler_findings(kRepo, out, 20);
  CHECK_MSG(f.size() == 2, std::to_string(f.size()) + " findings");
  if (f.size() != 2) return;
  CHECK(f[0].where.ends_with("CMakeLists.txt:34") &&
        f[0].what.find("links to:") != std::string::npos);
  CHECK(f[1].next == "clang-format -i sim/wind.cpp");
}

TEST(failed_tests_and_their_harness_lines_become_findings) {
  const std::string out = in_repo("tests/sim_wind_test.cpp") +
                          ":36: FAIL level 4.1 after 20 s, want inside (-1, 1)\n"
                          "  FAIL look: the mark left the picture\n"
                          "FAIL run: digest 1111111111111111, the baseline tests/digests "
                          "says 2222222222222222.\n"
                          "50% tests passed, 2 tests failed out of 4\n\n"
                          "The following tests FAILED:\n"
                          "\t  2 - sim_wind_test (Failed)\n"
                          "\t  4 - labs_smoke (Failed)\n";
  const auto f = test_findings(kRepo, out, "debug");
  CHECK_MSG(f.size() == 5, std::to_string(f.size()) + " findings");
  if (f.size() != 5) return;
  CHECK(f[0].where == "tests/sim_wind_test.cpp:36" && f[0].what.starts_with("level 4.1"));
  CHECK(f[1].where == "lab look" && f[1].next == "run it alone: ge lab look --smoke");
  // A digest off its baseline points at ge digest, the one command that checks and moves it.
  CHECK(f[2].where == "lab run" && f[2].next.starts_with("ge digest run checks it alone"));
  CHECK(f[3].what == "test sim_wind_test failed (Failed)" &&
        f[3].next.find("--filter=^sim_wind_test$") != std::string::npos);
  CHECK(f[4].where == "labs");
}

// `ge lab <id> --before=<ref>`: a ref that names no commit is refused before anything builds,
// and a real one is checked out into build/before/<sha>/ and built there (here it has no
// CMake presets, so its configure fails, and the finding says it was the ref's).
TEST(lab_before_checks_the_ref_out_into_a_worktree) {
  const fs::path repo = fs::temp_directory_path() / "ge_cli_build_test_before";
  fs::remove_all(repo);
  fs::create_directories(repo);
  const std::string git = "git -C \"" + repo.generic_string() + "\" ";
  const std::string quiet =
      " > \"" + (repo.parent_path() / "ge_cli_build_test_git.log").generic_string() + "\" 2>&1";
  CHECK(std::system((git + "init -q" + quiet).c_str()) == 0);
  CHECK(
      std::system((git + "-c user.name=t -c user.email=t@t commit -q --allow-empty -m one" + quiet)
                      .c_str()) == 0);
  BuildOptions o{repo, "debug", "", {}};
  LabOptions lab;
  lab.id = "run-clock";
  lab.before = "no-such-ref";
  const Outcome refused = run_lab(o, lab);
  CHECK(exit_of(refused) == 2);
  CHECK(!refused.findings.empty() &&
        refused.findings[0].what == "--before=no-such-ref names no commit");
  lab.before = "HEAD";
  const Outcome built = run_lab(o, lab);
  CHECK(exit_of(built) == 1);
  CHECK_MSG(!built.findings.empty() && built.findings[0].what.starts_with("at HEAD: "),
            built.findings.empty() ? std::string("no finding") : built.findings[0].what);
  bool tree = false;
  for (const auto& e : fs::directory_iterator(repo / "build" / "before"))
    tree = fs::exists(e.path() / ".git");
  CHECK(tree);
  fs::remove_all(repo);
}

// `ge lab before` refuses a tag that is not a plain name before it builds anything.
TEST(lab_before_needs_a_plain_tag) {
  LabBeforeOptions before;
  before.ids = {"world"};
  before.tag = "a/b";
  const Outcome o = run_lab_before({kRepo, "debug", "", {}}, before);
  CHECK(exit_of(o) == 2);
  CHECK(!o.findings.empty() && o.findings[0].what.find("not a plain name") != std::string::npos);
}

// `ge lab pair` with nothing to change between its two runs is refused before it builds.
TEST(lab_pair_needs_a_change) {
  LabPairOptions pair;
  pair.id = "world";
  const Outcome o = run_lab_pair({kRepo, "debug", "", {}}, pair);
  CHECK(exit_of(o) == 2);
  CHECK(!o.findings.empty() && o.findings[0].what.starts_with("a pair needs a change"));
  pair.flip = "ce_relief";
  pair.before = "ce_relief 0";
  const Outcome both = run_lab_pair({kRepo, "debug", "", {}}, pair);
  CHECK(exit_of(both) == 2);
  CHECK(!both.findings.empty() && both.findings[0].what.starts_with("--switch sets both sides"));
}
