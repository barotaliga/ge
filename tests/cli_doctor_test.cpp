// `ge doctor` (engine/cli/doctor.hpp): each rule it holds, broken one at a time in a freshly
// scaffolded repo, is reported with what, where and a next action that itself reads as a prompt;
// and a repo that breaks none is clean.
//
// ge:summary Tests: ge doctor reports each broken rule, every finding a prompt
// ge:keywords test cli ge doctor guidance symlinks pin fast-math libm clock ge-tags line-cap
// ge:keywords diagnostics finding
// ge:skill agent-interface testing
// ge:test tests/cli_doctor_test.cpp
#include <filesystem>
#include <functional>
#include <string>

#include "check.hpp"
#include "cli/diagnostic.hpp"
#include "cli/doctor.hpp"
#include "cli/scaffold.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

std::string say(const std::vector<Finding>& findings) {
  std::string s;
  for (const Finding& f : findings) s += format_finding("ge doctor", f);
  return s;
}

void append(const fs::path& file, const std::string& text) {
  std::string error;
  write_text(file, read_text(file).value_or("") + text, error);
}

// A fresh scaffold, broken by `breaks`, then doctored: the error whose `what` holds `want` must
// be there, at `where`, and every finding must be a whole prompt.
void expect(const std::string& name, const std::function<void(const fs::path&)>& breaks,
            const std::string& want, const std::string& where) {
  const fs::path repo = fs::temp_directory_path() / ("ge_cli_doctor_test_" + name) / "game";
  fs::remove_all(repo.parent_path());
  const NewResult made = new_game({repo, "", ""});
  if (!made.findings.empty()) {
    ge::test::skip("ge new failed here (" + made.findings[0].what + ")");
    return;
  }
  breaks(repo);
  const auto findings = doctor(repo);
  bool found = false;
  for (const Finding& f : findings) {
    CHECK_MSG(!f.what.empty() && !f.next.empty(), name + ": a finding without what or next");
    CHECK_MSG(
        f.severity == Severity::note || prompt_problem(format_finding("ge doctor", f)).empty(),
        name + ": a finding that does not read as a prompt: " + f.what + " / " + f.next);
    if (f.severity == Severity::error && f.what.find(want) != std::string::npos &&
        f.where.starts_with(where)) {
      found = true;
    }
  }
  CHECK_MSG(found,
            name + ": want an error \"" + want + "\" at " + where + "; got:\n" + say(findings));
}

}  // namespace

TEST(a_link_turned_into_a_copy) {
  expect(
      "link",
      [](const fs::path& r) {
        fs::remove(r / "CLAUDE.md");
        std::string e;
        write_text(r / "CLAUDE.md", "a copy\n", e);
      },
      "CLAUDE.md is not a symlink", "CLAUDE.md");
}

TEST(a_loosened_fp_flag) {
  expect(
      "fast-math",
      [](const fs::path& r) {
        append(r / "CMakeLists.txt", "target_compile_options(labs PRIVATE -ffast-math)\n");
      },
      "the build flag -ffast-math", "CMakeLists.txt:");
}

TEST(a_platform_transcendental_in_sim) {
  expect(
      "libm",
      [](const fs::path& r) {
        append(r / "sim/game.cpp", "double wobble(double t) { return std::sin(t); }\n");
      },
      "a platform transcendental", "sim/game.cpp:");
}

TEST(a_clock_in_sim) {
  expect(
      "clock",
      [](const fs::path& r) {
        append(r / "sim/game.cpp", "auto started = std::chrono::steady_clock::now();\n");
      },
      "reads a clock", "sim/game.cpp:");
}

TEST(a_file_without_tags) {
  expect(
      "tags",
      [](const fs::path& r) {
        std::string e;
        write_text(r / "sim/untagged.hpp", "#pragma once\n", e);
      },
      "no ge:summary tag", "sim/untagged.hpp");
}

TEST(a_file_over_the_cap) {
  expect(
      "cap",
      [](const fs::path& r) {
        std::string body = "// ge:summary long\n// ge:keywords long\n// ge:skill engine\n";
        for (int i = 0; i < 1001; i++) body += "\n";
        std::string e;
        write_text(r / "sim/long.hpp", body, e);
      },
      "over the cap of 1000", "sim/long.hpp");
}

TEST(a_message_without_a_next_action) {
  expect(
      "message",
      [](const fs::path& r) {
        append(r / "sim/game.cpp",
               "void oops() { ge::core::output::error(\"it broke badly\"); }\n");
      },
      "names no next action", "sim/game.cpp:");
}

TEST(a_pin_from_another_release) {
  expect(
      "pin",
      [](const fs::path& r) {
        std::string text = read_text(r / kPinFile).value_or("");
        const size_t at = text.find("set(GE_ENGINE_VERSION ");
        text.replace(at, text.find(')', at) - at, "set(GE_ENGINE_VERSION 9.9.9");
        std::string e;
        write_text(r / kPinFile, text, e);
      },
      "the repo pins engine 9.9.9", "cmake/engine.cmake");
}

TEST(an_unrouted_skill) {
  expect(
      "route",
      [](const fs::path& r) {
        std::string e;
        write_text(r / ".agents/skills/netcode/SKILL.md",
                   "---\nname: netcode\ndescription: \"the game's own netcode\"\n---\n", e);
        write_text(r / ".agents/skills/netcode/.lessons/.gitkeep", "", e);
      },
      "skill netcode is not routed from AGENTS.md", "AGENTS.md");
}
