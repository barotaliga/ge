// ge tool's pure half: the plugins that carry a tool found in a repo, and a tool's JSON answer
// read into an outcome (a clean answer, a refusal as a finding, a usage error, no answer at all).
//
// ge:summary Tests ge tool's listing and how it reads a tool's JSON answer
// ge:keywords test cli tool plugin answer finding usage
// ge:skill agent-interface
#include <filesystem>
#include <fstream>

#include "check.hpp"
#include "cli/tool.hpp"

namespace fs = std::filesystem;

TEST(tool_lists_plugins_with_a_tool_source) {
  const fs::path repo = fs::temp_directory_path() / "ge_cli_tool_test";
  fs::remove_all(repo);
  fs::create_directories(repo / "plugins/alpha/tool");
  fs::create_directories(repo / "plugins/beta");
  fs::create_directories(repo / "plugins/gamma/tool");  // a tool folder with no source
  std::ofstream(repo / "plugins/alpha/tool/main.cpp") << "int main() {}\n";
  const auto tools = ge::cli::plugin_tools(repo);
  CHECK(tools.size() == 1 && tools[0] == "alpha");
  fs::remove_all(repo);
}

TEST(tool_answer_reads_the_last_json_line) {
  const auto ok = ge::cli::tool_answer("alpha", 0, "working\n{\"ok\":true,\"n\":3}\n");
  CHECK(ok.findings.empty());
  CHECK(ok.result == "{\"ok\":true,\"n\":3}");
  CHECK(ge::cli::exit_of(ok) == 0);

  const auto refused = ge::cli::tool_answer(
      "alpha", 1, "{\"ok\":false,\"error\":\"the \\\"pack\\\" is unknown\"}\n");
  CHECK(refused.findings.size() == 1);
  CHECK(refused.findings[0].what == "the \"pack\" is unknown");
  CHECK(ge::cli::exit_of(refused) == 1);

  const auto usage = ge::cli::tool_answer("alpha", 2, "{\"ok\":false,\"error\":\"bad flag\"}");
  CHECK(ge::cli::exit_of(usage) == 2);

  const auto silent = ge::cli::tool_answer("alpha", 3, "crashed\n");
  CHECK(silent.findings.size() == 1 && ge::cli::exit_of(silent) == 1);
}
