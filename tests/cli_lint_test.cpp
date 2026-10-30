// `ge lint`'s parts that need no build (engine/cli/lint.hpp): clang-tidy's diagnostics read as
// findings at their repository-relative file:line with the check named and its page as the next
// step, a header's finding reported once though every includer repeats it, the cap, and the files
// a build has no compile command for kept from clang-tidy.
//
// ge:summary Test: ge lint's clang-tidy output read as findings, and a check's documentation url
// ge:keywords test cli lint clang-tidy lint_findings tidy_check_url split_by_compile_db
// ge:skill testing agent-interface
#include <string>

#include "check.hpp"
#include "cli/lint.hpp"

using namespace ge::cli;

TEST(a_diagnostic_is_a_finding_at_its_line_with_its_check) {
  const std::string out =
      "12 warnings generated.\n"
      "/repo/games/x/sim/step.cpp:41:7: error: 'v' used after it was moved "
      "[bugprone-use-after-move,-warnings-as-errors]\n"
      "  41 |   use(v);\n"
      "     |       ^\n"
      "/repo/games/x/sim/step.cpp:30:3: note: move occurred here\n"
      "/repo/games/x/sim/grid.hpp:9:12: warning: Called C++ object pointer is null "
      "[clang-analyzer-core.CallAndMessage]\n";
  const auto f = lint_findings("/repo", out, 10);
  CHECK(f.size() == 2);
  CHECK(f[0].where == "games/x/sim/step.cpp:41");
  CHECK(f[0].what == "'v' used after it was moved [bugprone-use-after-move]");
  CHECK(f[0].severity == Severity::error);
  CHECK(f[0].next.find("https://clang.llvm.org/extra/clang-tidy/checks/bugprone/"
                       "use-after-move.html") != std::string::npos);
  CHECK(f[1].where == "games/x/sim/grid.hpp:9");
}

TEST(a_headers_finding_through_two_includers_is_one_finding) {
  const std::string line =
      "/repo/games/x/a.hpp:3:1: error: copy of a large value [performance-unnecessary-copy-"
      "initialization]\n";
  CHECK(lint_findings("/repo", line + line, 10).size() == 1);
}

TEST(past_the_cap_the_rest_are_counted) {
  std::string out;
  for (int i = 1; i <= 5; i++) {
    out += "/repo/a.cpp:" + std::to_string(i) + ":1: error: m [bugprone-x]\n";
  }
  const auto f = lint_findings("/repo", out, 3);
  CHECK(f.size() == 4);
  CHECK(f[3].severity == Severity::note && f[3].what == "2 more clang-tidy findings");
}

TEST(a_checks_page_is_under_its_group) {
  CHECK(tidy_check_url("performance-for-range-copy") ==
        "https://clang.llvm.org/extra/clang-tidy/checks/performance/for-range-copy.html");
  CHECK(tidy_check_url("clang-analyzer-core.NullDereference") ==
        "https://clang.llvm.org/extra/clang-tidy/checks/clang-analyzer/core.NullDereference.html");
}

TEST(a_file_the_build_does_not_compile_is_named_not_linted) {
  const std::string db = R"([
{ "directory": "/repo/build/debug", "command": "c++ -c /repo/engine/core/prng.cpp",
  "file": "/repo/engine/core/prng.cpp", "output": "x.o" },
{ "directory": "C:/repo/build", "file": "C:\\repo\\engine\\ui\\paint.cpp" }
])";
  const auto [kept, missing] =
      split_by_compile_db(db, "/repo", {"engine/core/prng.cpp", "engine/render/sky.cpp"});
  CHECK(kept.size() == 1 && kept[0] == "engine/core/prng.cpp");
  CHECK(missing.size() == 1 && missing[0] == "engine/render/sky.cpp");
  // A prefix is not a match: prng.cpp is not engine/core/prn.cpp.
  CHECK(split_by_compile_db(db, "/repo", {"engine/core/prn.cpp"}).second.size() == 1);
  CHECK(split_by_compile_db(db, "C:/repo", {"engine/ui/paint.cpp"}).first.size() == 1);
}
