// THE DIAGNOSTICS CONVENTION, HELD (docs/architecture.md § Diagnostics): every error and warning
// the engine, its plugins, its games, its build and the game scaffold can print reads as a
// prompt for an agent (what failed, where, the next action). The rule and the lexers are
// engine/cli/diagnostic.hpp's; this file tests them, then runs them over the whole tree, and
// every message ge itself writes passes the same rule.
//
// ge:summary Tests: the prompt rule, the lexers, and every message in the tree read as prompts
// ge:keywords test diagnostics prompt actionable message lexer fatal-error agent-output
// ge:skill agent-interface testing
// ge:test tests/cli_diagnostics_test.cpp
#include <algorithm>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/diagnostic.hpp"
#include "cli/doctor.hpp"
#include "cli/finding.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

TEST(a_prompt_names_a_next_action) {
  CHECK(prompt_problem("gpu: the device was lost; skill: gpu.").empty());
  CHECK(prompt_problem("sim: seed 38 left the field; next: make lab LAB=run").empty());
  CHECK(prompt_problem("ge: VERSION is wrong. Restore it; see docs/publishing.md").empty());
  CHECK(!prompt_problem("gpu: the device was lost").empty());
  CHECK(!prompt_problem("failed; skill:gpu").empty());  // two words: no room to say what
  CHECK(!prompt_problem("").empty());
}

TEST(cpp_sites_skip_comments_and_gather_literals) {
  const std::string src =
      "// out::error(\"in a comment\");\n"
      "void f() {\n"
      "  out::error(\"gpu: \" + name + \" (a) failed; \"\n"
      "             \"skill: gpu\");\n"
      "  ge::core::output::warn(R\"(raw \"quoted\" text)\");\n"
      "  lab.fail(\"what\", \"next\");\n"
      "  error(\"a local function, not the channel\");\n"
      "  out::warn(message);\n"
      "}\n";
  const auto sites = diagnostic_sites(src, SourceKind::cpp);
  CHECK_MSG(sites.size() == 3, "want 3 sites, got " + std::to_string(sites.size()) +
                                   "; next: read cpp_sites in engine/cli/diagnostic.cpp");
  if (sites.size() != 3) return;
  CHECK(sites[0].line == 3 && sites[0].call == "out::error");
  CHECK(sites[0].text == "gpu:  (a) failed; skill: gpu");
  CHECK(sites[1].call == "ge::core::output::warn" && sites[1].text == "raw \"quoted\" text");
  CHECK(sites[2].call == ".fail" && sites[2].text == "whatnext");
}

TEST(cmake_sites_read_message_modes) {
  const std::string src =
      "# message(FATAL_ERROR \"commented\")\n"
      "if(X)\n"
      "  message(FATAL_ERROR \"ge: X is on (it must not be); \"\n"
      "    \"configure with -DX=OFF; skill: build.\")\n"
      "  message(STATUS \"just status\")\n"
      "  MESSAGE(WARNING \"lower case calls count too\")\n"
      "endif()\n";
  const auto sites = diagnostic_sites(src, SourceKind::cmake);
  CHECK_MSG(sites.size() == 2, "want 2 sites, got " + std::to_string(sites.size()) +
                                   "; next: read cmake_sites in engine/cli/diagnostic.cpp");
  if (sites.size() != 2) return;
  CHECK(sites[0].line == 3 && sites[0].call == "message(FATAL_ERROR");
  CHECK(sites[0].text == "ge: X is on (it must not be); configure with -DX=OFF; skill: build.");
  CHECK(sites[1].call == "message(WARNING");
}

// Every message in the engine's own tree, the scaffold included, reads as a prompt.
TEST(every_message_in_the_tree_reads_as_a_prompt) {
  const fs::path root = GE_SOURCE_DIR;
  int files = 0;
  auto scan = [&](const fs::path& file) {
    files++;
    for (const Finding& f : check_diagnostics_in(root, file)) {
      ::ge::test::fail(__FILE__, __LINE__, f.where + ": " + f.what + "; next: " + f.next);
    }
  };
  for (const char* dir : {"engine", "plugins", "games", "scaffold", "cmake"}) {
    for (const auto& e : fs::recursive_directory_iterator(root / dir)) {
      if (!e.is_regular_file()) continue;
      std::string name = e.path().filename().string();
      if (name.ends_with(".in")) name.resize(name.size() - 3);
      const std::string ext = fs::path(name).extension().string();
      if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cmake" ||
          name == "CMakeLists.txt") {
        scan(e.path());
      }
    }
  }
  scan(root / "CMakeLists.txt");
  CHECK_MSG(files > 100, "scanned only " + std::to_string(files) +
                             " files; next: check GE_SOURCE_DIR in tests/CMakeLists.txt");
}

// A finding always carries all three parts, and its printed shape says each one.
TEST(a_finding_prints_what_where_and_next) {
  const std::string text = format_finding(
      "ge doctor",
      {Severity::error, "the pin is gone", "cmake/engine.cmake", "restore it; skill: engine"});
  CHECK(text ==
        "ge doctor: error: the pin is gone\n    where: cmake/engine.cmake\n"
        "    next: restore it; skill: engine\n");
  CHECK(format_finding("ge", {Severity::note, "x y z", "", "n"}).find("where: the repository") !=
        std::string::npos);
}
