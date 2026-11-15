// Tests: the map group run through the brush tool in this process, as a game's CLI runs it: `map
// info` counts a fixture map, `map ents` lists by class, `map slice` draws its grid as the text,
// `map liquids` answers a dry map; a missing map and `map check` without a search path are
// refusals whose next step is spelled with the calling program's name; the rows hand the tool
// their words and flags. The tool itself is held by tests/brush_tool_test.cpp; secli offering the
// rows by tests/gamecli_cli_test.cpp.
//
// ge:summary Tests: the map group's rows over the brush tool and the reading of its answer
// ge:keywords test toolkit map bsp brush info ents slice liquids check answer tool-runner
// ge:skill agent-interface brush testing
// ge:test tests/toolkit_map_test.cpp
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "brush/cli/commands.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "toolkit/map.hpp"

using namespace ge::toolkit;
namespace br = ge::brush;
namespace fs = std::filesystem;

namespace {

std::string fixture_file() {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error));
  const fs::path dir = fs::temp_directory_path() / "ge_toolkit_map_test";
  fs::create_directories(dir);
  const fs::path p = dir / "one.bsp";
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  return p.generic_string();
}

Outcome run(const std::string& command, std::vector<std::string> words,
            std::vector<std::pair<std::string, std::string>> flags = {},
            std::string program = "secli") {
  const Row* row = find_row(command);
  CHECK(row != nullptr);
  if (row == nullptr) return {};
  CHECK(row->tool == "brush");
  Call c;
  c.program = std::move(program);
  c.words = std::move(words);
  c.flags = std::move(flags);
  c.tool = br::cli::run_tool;
  return run_row(*row, c);
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(map_info_counts_the_map_and_points_at_its_entities) {
  const std::string map = fixture_file();
  const Outcome o = run("map info", {map});
  CHECK(exit_of(o) == 0);
  CHECK(has(o.summary, ", family 29: ") && has(o.summary, "surfaces"));
  CHECK(has(o.text, "entities "));
  CHECK(has(o.result, "\"ok\":true"));
  CHECK(o.next.rfind("secli map ents " + map, 0) == 0);
}

TEST(map_ents_slice_and_liquids_answer_as_text) {
  const std::string map = fixture_file();
  const Outcome ents = run("map ents", {map}, {{"class", "worldspawn"}});
  CHECK(exit_of(ents) == 0);
  CHECK(ents.summary == "1 entities in " + map);
  CHECK(has(ents.text, "worldspawn"));
  const Outcome slice = run("map slice", {map}, {{"z", "0"}, {"step", "16"}});
  CHECK(exit_of(slice) == 0);
  CHECK(has(slice.summary, "cut at z=0"));
  CHECK(!slice.text.empty());
  const Outcome liquids = run("map liquids", {map});
  CHECK(exit_of(liquids) == 0);
  CHECK(!liquids.summary.empty());
}

TEST(a_missing_map_and_check_without_a_search_path_are_refused_in_the_callers_name) {
  const Outcome missing = run("map info", {"no/such.bsp"}, {}, "ge");
  CHECK(exit_of(missing) == 1);
  CHECK(missing.findings.size() == 1);
  CHECK(has(missing.findings[0].what, "cannot open no/such.bsp"));
  CHECK(missing.findings[0].where == "no/such.bsp");
  CHECK(has(missing.findings[0].next, "ge map check"));
  const Outcome check = run("map check", {});
  CHECK(exit_of(check) == 2);
  CHECK(has(check.findings.at(0).what, "--root"));
}

TEST(the_rows_hand_the_tool_the_command_words_and_flags) {
  const Row* row = find_row("map slice");
  CHECK(row != nullptr);
  if (row == nullptr) return;
  Call c;
  c.program = "ge";
  c.words = {"dm4.bsp"};
  c.flags = {{"z", "64"}, {"step", "32"}};
  std::vector<std::string> seen;
  c.tool = [&](std::string_view, std::string_view tool, const std::vector<std::string>& args) {
    CHECK(tool == "brush");
    seen = args;
    ToolRun r;
    r.output =
        "{\"ok\":true,\"map\":\"dm4.bsp\",\"plane\":\"z=64\",\"step\":32,\"grid\":[\"#.#\"]}\n";
    return r;
  };
  const Outcome o = run_row(*row, c);
  CHECK((seen == std::vector<std::string>{"slice", "dm4.bsp", "--z=64", "--step=32"}));
  CHECK(o.text == "#.#\n");
  Call bare;
  bare.program = "fall_line_cli";
  bare.words = {"dm4.bsp"};
  const Outcome none = run_row(*row, bare);
  CHECK(exit_of(none) == 2);
}
